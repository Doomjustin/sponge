#ifndef SPONGE_LEVELDB_WRITABLE_FILE_H
#define SPONGE_LEVELDB_WRITABLE_FILE_H

#include <fstream>
#include <span>

#include <sponge/leveldb/status.h>

namespace spg::leveldb {

class StdWritableFile {
public:
    explicit StdWritableFile(std::string_view filename);

    StdWritableFile(const StdWritableFile&) = delete;
    auto operator=(const StdWritableFile&) -> StdWritableFile& = delete;

    StdWritableFile(StdWritableFile&&) noexcept = default;
    auto operator=(StdWritableFile&&) noexcept -> StdWritableFile& = default;

    ~StdWritableFile();

    auto append(std::span<const std::byte> data) -> Status;

    auto flush() -> Status;

    auto sync() -> Status;

private:
    std::ofstream stream_;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_WRITABLE_FILE_H
