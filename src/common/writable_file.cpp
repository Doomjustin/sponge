#include <sponge/writable_file.h>

namespace spg {

StdWritableFile::StdWritableFile(std::string_view filename)
  : StdWritableFile{ std::filesystem::path{ filename } }
{}

StdWritableFile::StdWritableFile(std::filesystem::path filename)
  : filename_{ std::move(filename) },
    stream_{ filename_, std::ios::app | std::ios::binary | std::ios::out }
{}

StdWritableFile::~StdWritableFile()
{
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

auto StdWritableFile::append(std::span<const std::byte> data) -> bool
{
    if (!stream_.is_open())
        return false;

    stream_.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (stream_.bad())
        return false;

    return true;
}

auto StdWritableFile::flush() -> bool
{
    if (!stream_.is_open())
        return false;

    stream_.flush();
    if (stream_.bad())
        return false;

    return true;
}

auto StdWritableFile::sync() -> bool
{
    // std::ofstream doesn't provide a way to sync the underlying file descriptor,
    // so we rely on flush() to do that. This may not be sufficient for all platforms,
    // but it's the best we can do with the standard library.
    return flush();
}

void StdWritableFile::close()
{
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

void StdWritableFile::reopen(std::string_view filename)
{
    reopen(std::filesystem::path{ filename });
}

void StdWritableFile::reopen(std::filesystem::path path)
{
    close();
    filename_ = std::move(path);
    stream_.open(filename_, std::ios::app | std::ios::binary | std::ios::out);
}

} // namespace spg