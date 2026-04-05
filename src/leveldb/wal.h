#ifndef SPONGE_LEVELDB_WAL_H
#define SPONGE_LEVELDB_WAL_H

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <boost/asio.hpp>

#include <sponge/leveldb/formats.h>

namespace spg::leveldb::wal {

namespace asio = boost::asio;
namespace fs = std::filesystem;

inline constexpr size_t HEADER_SIZE = 7;
inline constexpr size_t LOG_BLOCK_SIZE = 32 * 1024; // 32KB


class Writer {
public:
    Writer(asio::any_io_executor executor, const fs::path& path);

    Writer(const Writer&) = delete;
    auto operator=(const Writer&) -> Writer& = delete;

    Writer(Writer&&) = default;
    auto operator=(Writer&&) -> Writer& = default;

    ~Writer() = default;

    auto async_append(std::string_view record) -> asio::awaitable<void>;

    void sync();

private:
    asio::stream_file file_;
    size_t block_offset_ = 0;

    auto async_write(asio::const_buffer buffer) -> asio::awaitable<void>;
    auto emit_physical_record(std::string_view payload, RecordType type) -> asio::awaitable<void>;
};


class Reader {
public:
    class Iterator;

    explicit Reader(asio::stream_file& file, const fs::path& path, bool paranoid_checks = false);

    Reader(const Reader&) = delete;
    auto operator=(const Reader&) -> Reader& = delete;

    Reader(Reader&&) = delete;
    auto operator=(Reader&&) -> Reader& = delete;

    ~Reader() = default;

    auto begin() -> Iterator;

    auto end() -> Iterator;

private:
    asio::stream_file& file_;
    bool paranoid_checks_;

    bool eof_ = false;
    std::array<char, LOG_BLOCK_SIZE> buffer_{};
    size_t buffer_cursor_ = 0;
    size_t buffer_size_ = 0;

    std::string scratch_;
    std::string_view current_record_;

    auto read_next() -> bool;

    auto read_physical_record() -> std::optional<std::pair<std::string_view, RecordType>>;

    void report_corruption(std::string_view message);
};


class Reader::Iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = const std::string_view*;
    using reference = const std::string_view&;

    Iterator() = default;

    explicit Iterator(Reader* reader)
      : reader_{ reader }
    {}

    auto operator*() const-> std::string_view
    {
        return reader_->current_record_;
    }

    auto operator++() -> Iterator&
    {
        if (!reader_->read_next())
            reader_ = nullptr;

        return *this;
    }

    auto operator++(int) -> Iterator
    {
        Iterator temp = *this;
        ++(*this);
        return temp;
    }

    auto operator==(const Iterator& other) const noexcept -> bool
    {
        return reader_ == other.reader_;
    }

    auto operator!=(const Iterator& other) const noexcept -> bool
    {
        return !(*this == other);
    }

private:
    Reader* reader_ = nullptr;
};

} // namespace spg::leveldb::wal

#endif // SPONGE_LEVELDB_WAL_H
