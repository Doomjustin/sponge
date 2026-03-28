#ifndef SPONGE_WRITABLE_FILE_H
#define SPONGE_WRITABLE_FILE_H

#include <fstream>
#include <span>

namespace spg {

class StdWritableFile {
public:
    explicit StdWritableFile(std::string_view filename);

    StdWritableFile(const StdWritableFile&) = delete;
    auto operator=(const StdWritableFile&) -> StdWritableFile& = delete;

    StdWritableFile(StdWritableFile&&) noexcept = default;
    auto operator=(StdWritableFile&&) noexcept -> StdWritableFile& = default;

    ~StdWritableFile();

    auto append(std::span<const std::byte> data) -> bool;

    auto flush() -> bool;

    auto sync() -> bool;

private:
    std::ofstream stream_;
};

} // namespace spg

#endif // SPONGE_WRITABLE_FILE_H
