#include "wal.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

#include <sponge/coding.h>
#include <sponge/leveldb/exceptions.h>
#include <sponge/leveldb/formats.h>

#include "crc.h"

namespace spg::leveldb::wal {

static auto WRITE_OPTIONS = asio::file_base::append | asio::file_base::create | asio::file_base::write_only;

Writer::Writer(asio::any_io_executor executor, const fs::path& path)
  : file_{ executor, path, WRITE_OPTIONS }
{}

auto Writer::async_append(std::string_view record) -> asio::awaitable<void> 
{
    bool begin = true;

    do {
        const auto leftover = LOG_BLOCK_SIZE - block_offset_;
        if (leftover < HEADER_SIZE) {
            static constexpr std::string_view zeroes = "\0\0\0\0\0\0\0";
            co_await async_write(asio::const_buffer(zeroes.data(), leftover));
            block_offset_ = 0;
        }

        const auto avail = LOG_BLOCK_SIZE - block_offset_ - HEADER_SIZE;
        const auto to_write = std::min(avail, record.size());
        const bool end = (to_write == record.size());

        RecordType type;
        if (begin && end) type = RecordType::Full;
        else if (begin) type = RecordType::First;
        else if (end) type = RecordType::Last;
        else type = RecordType::Middle;

        co_await emit_physical_record(record.substr(0, to_write), type);

        record.remove_prefix(to_write);
        begin = false;
    } while (!record.empty());
}

void Writer::sync()
{
    boost::system::error_code ec;
    file_.sync_all(ec);

    if (ec)
        throw IOException{ "WAL log sync failed {}.", ec.message() };
}

auto Writer::async_write(asio::const_buffer buffer) -> asio::awaitable<void>
{
    boost::system::error_code ec;
    co_await asio::async_write(file_, buffer, asio::redirect_error(asio::use_awaitable, ec));

    if (ec)
        throw IOException{ "WAL log write failed {}.", ec.message() };
}

auto Writer::emit_physical_record(std::string_view payload, RecordType type) -> asio::awaitable<void>
{
    std::array<char, HEADER_SIZE> header;

    // [0..3] 存储 checksum，计算时 type 也要算入 checksum
    auto checksum = crc::compute(std::to_underlying(type), payload, crc::add_mask);
    auto iter = fixed::encode(header.begin(), checksum);
    // [4..5] 存储 payload 长度
    iter = fixed::encode(iter, static_cast<uint16_t>(payload.size()));
    // [6] 存储 record type（单字节）
    fixed::encode(iter, std::to_underlying(type));

    std::array<asio::const_buffer, 2> buffers{ asio::buffer(header), asio::buffer(payload) };

    boost::system::error_code ec;
    co_await asio::async_write(file_, buffers, asio::redirect_error(asio::use_awaitable, ec));

    if (ec)
        throw IOException{ "WAL log write failed {}.", ec.message() };

    block_offset_ += HEADER_SIZE + payload.size();
}


Reader::Reader(asio::stream_file& file, const fs::path& path, bool paranoid_checks)
  : file_{ file }, 
    paranoid_checks_{ paranoid_checks }
{}

auto Reader::read_next() -> bool
{
    scratch_.clear();
    bool in_fragmented_record = false;

    while (true) {
        auto record_opt = read_physical_record();
        if (!record_opt) {
            if (in_fragmented_record && !eof_)
                report_corruption("Unexpected end of WAL log in the middle of a record");

            return false;
        }

        const auto [fragment, type] = *record_opt;
        switch (type) {
        case RecordType::Full:
            if (in_fragmented_record)
                report_corruption("Unexpected full record while reading fragmented record");

            current_record_ = fragment;
            return true;

        case RecordType::First:
            if (in_fragmented_record)
                report_corruption("Unexpected first record while reading fragmented record");
            scratch_ = fragment;
            in_fragmented_record = true;
            break;

        case RecordType::Middle:
            if (!in_fragmented_record)                
                report_corruption("Unexpected middle record without preceding first record");
            else 
                scratch_ += fragment;

            break;

        case RecordType::Last:
            if (!in_fragmented_record) {
                report_corruption("Unexpected last record without preceding first record");
            }
            else {
                scratch_ += fragment;
                current_record_ = scratch_;
                in_fragmented_record = false;
                return true;
            }
            break;
            
        case RecordType::Zero:
            buffer_cursor_ = buffer_size_; // 这一段是 padding，跳过
            break;
        default:
            std::unreachable();
        }
    }
}

auto Reader::read_physical_record() -> std::optional<std::pair<std::string_view, RecordType>>
{
    while (true) {
        if (buffer_size_ - buffer_cursor_ < HEADER_SIZE) {
            if (eof_) return {};

            buffer_cursor_ = 0;
            boost::system::error_code ec;
            buffer_size_ = asio::read(file_, asio::buffer(buffer_), asio::transfer_exactly(LOG_BLOCK_SIZE), ec);

            if (ec == asio::error::eof) {
                if (buffer_size_ == 0) {
                    eof_ = true;
                    return {};
                }

                // 末尾短块：保留已读数据继续解析
                eof_ = true;
            }
            else if (ec) {
                throw IOException{ "WAL log read failed {}.", ec.message() };
            }
        }

        const auto* header = buffer_.data() + buffer_cursor_;
        const auto [length, _] = fixed::decode<uint16_t>(&header[4]);

        if (header[4] == 0 && header[5] == 0 && header[6] == 0) {
            // 这一段是 padding，跳过
            buffer_cursor_ = buffer_size_;
            continue;
        }

        if (length + HEADER_SIZE > buffer_size_ - buffer_cursor_) {
            buffer_cursor_ = buffer_size_;
            report_corruption("Record length exceeds block size (Torn Write)");
            return {};
        }

        auto type = static_cast<RecordType>(static_cast<uint8_t>(header[6]));
        std::string_view payload{ header + HEADER_SIZE, length };
        auto [expected_crc, unused] = fixed::decode<uint32_t>(header);
        if (!crc::verify(crc::masked(expected_crc), std::to_underlying(type), payload)) {
            buffer_cursor_ = buffer_size_;
            report_corruption("Checksum mismatch");
            return {};
        }

        buffer_cursor_ += HEADER_SIZE + length;
        return std::make_pair(payload, type);
    }
}

void Reader::report_corruption(std::string_view message)
{
    if (paranoid_checks_)
        throw CorruptionException{ "WAL log corruption: {}.", message };

    SPDLOG_WARN("WAL log corruption: {}.", message);
}

auto Reader::begin() -> Iterator
{
    if (read_next())
        return Iterator{ this };

    return end();
}

auto Reader::end() -> Iterator
{
    return Iterator{};
}

} // namespace spg::leveldb::wal
