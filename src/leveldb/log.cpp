#include "log.h"

#include <algorithm>
#include <utility>

#include <sponge/coding.h>
#include <sponge/leveldb/exceptions.h>
#include <sponge/leveldb/formats.h>

#include "crc.h"

namespace spg::leveldb {

static auto FILE_OPTIONS = asio::file_base::append | asio::file_base::create | asio::file_base::write_only;

Writer::Writer(asio::any_io_executor executor, const fs::path& path)
  : file_{ executor, path, FILE_OPTIONS }
{}

auto Writer::async_append(std::string_view record) -> asio::awaitable<void> 
{
    bool begin = true;

    do {
        const auto leftover = BLOCK_SIZE - block_offset_;
        if (leftover < HEADER_SIZE) {
            static constexpr std::string_view zeroes = "\0\0\0\0\0\0\0";
            co_await async_write(asio::const_buffer(zeroes.data(), leftover));
            block_offset_ = 0;
        }

        const auto avail = BLOCK_SIZE - block_offset_ - HEADER_SIZE;
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

auto Writer::sync() -> asio::awaitable<void>
{
    boost::system::error_code ec;
    file_.sync_all(ec);

    if (ec)
        throw IOException{ "WAL log sync failed {}.", ec.message() };

    co_return;
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

    header[4] = static_cast<char>(payload.size() & 0xFF);
    header[5] = static_cast<char>(payload.size() >> 8);
    header[6] = static_cast<char>(type);

    auto checksum = crc::compute(std::to_underlying(type), payload, crc::add_mask);
    fixed::encode(header.begin(), checksum);

    std::array<asio::const_buffer, 2> buffers{ asio::buffer(header), asio::buffer(payload) };

    boost::system::error_code ec;
    co_await asio::async_write(file_, buffers, asio::redirect_error(asio::use_awaitable, ec));

    if (ec)
        throw IOException{ "WAL log write failed {}.", ec.message() };

    block_offset_ += HEADER_SIZE + payload.size();
}

} // namespace spg::leveldb
