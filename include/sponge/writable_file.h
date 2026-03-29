#ifndef SPONGE_WRITABLE_FILE_H
#define SPONGE_WRITABLE_FILE_H

#include <filesystem>
#include <fstream>
#include <span>

namespace spg {

class StdWritableFile {
public:
    explicit StdWritableFile(std::string_view filename);

    explicit StdWritableFile(std::filesystem::path filename);

    StdWritableFile(const StdWritableFile&) = delete;
    auto operator=(const StdWritableFile&) -> StdWritableFile& = delete;

    StdWritableFile(StdWritableFile&&) noexcept = default;
    auto operator=(StdWritableFile&&) noexcept -> StdWritableFile& = default;

    ~StdWritableFile();

    auto append(std::span<const std::byte> data) -> bool;

    auto flush() -> bool;

    auto sync() -> bool;

    void close();

    void reopen(std::string_view filename);

    void reopen(std::filesystem::path path);

    auto name() const -> std::string
    {
        return filename_.string();
    }

    auto path() const -> std::filesystem::path
    {
        return filename_;
    }

private:
    std::filesystem::path filename_;
    std::ofstream stream_;
};

} // namespace spg

#endif // SPONGE_WRITABLE_FILE_H
