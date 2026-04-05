#ifndef SPONGE_LEVELDB_LOG_H
#define SPONGE_LEVELDB_LOG_H

#include <cstddef>
#include <filesystem>
#include <string_view>

#include <boost/asio.hpp>

#include <sponge/leveldb/formats.h>

namespace spg::leveldb {

namespace asio = boost::asio;
namespace fs = std::filesystem;

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
    static constexpr size_t HEADER_SIZE = 7;

    asio::stream_file file_;
    size_t block_offset_ = 0;

    auto async_write(asio::const_buffer buffer) -> asio::awaitable<void>;
    auto emit_physical_record(std::string_view payload, RecordType type) -> asio::awaitable<void>;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_LOG_H
