#ifndef SPONGE_LEVELDB_EXCEPTIONS_H
#define SPONGE_LEVELDB_EXCEPTIONS_H

#include <stdexcept>
#include <string_view>
#include <utility>

#include <sponge/format.h>

namespace spg::leveldb {

class LevelDBException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


class IOException : public LevelDBException {
public:
    template<typename... Args>
    explicit IOException(const std::string_view fmt, Args&&... args)
      : LevelDBException{ xformat("IO Exception: {}", xformat(fmt, std::forward<Args>(args)...)) }
    {}
};


class InvalidArgumentException : public LevelDBException {
public:
    template<typename... Args>
    explicit InvalidArgumentException(const std::string_view fmt, Args&&... args)
      : LevelDBException{ xformat("Invalid Argument Exception: {}", xformat(fmt, std::forward<Args>(args)...)) }
    {}
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_EXCEPTIONS_H
