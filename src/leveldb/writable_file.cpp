#include "writable_file.h"

namespace spg::leveldb {
    
StdWritableFile::StdWritableFile(std::string_view filename)
    : stream_{ filename.data(), std::ios::app | std::ios::binary | std::ios::out }
{}

StdWritableFile::~StdWritableFile()
{
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

auto StdWritableFile::append(std::span<const std::byte> data) -> Status
{
    if (!stream_.is_open())
        return Status::io_error("failed to open file for writing");

    stream_.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (stream_.bad())
        return Status::io_error("failed to write to file");

    return Status::ok();
}

auto StdWritableFile::flush() -> Status
{
    if (!stream_.is_open())
        return Status::io_error("failed to open file for writing");

    stream_.flush();
    if (stream_.bad())
        return Status::io_error("failed to flush file");

    return Status::ok();
}

auto StdWritableFile::sync() -> Status
{
    // std::ofstream doesn't provide a way to sync the underlying file descriptor,
    // so we rely on flush() to do that. This may not be sufficient for all platforms,
    // but it's the best we can do with the standard library.
    return flush();
}

} // namespace spg::leveldb
