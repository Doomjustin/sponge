#ifndef SPONGE_LEVELDB_ITERATOR_H
#define SPONGE_LEVELDB_ITERATOR_H

#include <string_view>

#include "status.h"

namespace spg::leveldb {

struct SeekFromFirstTag {};
struct SeekFromLastTag {};

static constexpr SeekFromFirstTag from_first{};
static constexpr SeekFromLastTag from_last{};


struct Iterator {
    virtual ~Iterator() = default;

    virtual void seek(SeekFromFirstTag t) = 0;

    virtual void seek(SeekFromLastTag t) = 0;

    virtual void seek(std::string_view target) = 0;

    virtual void next() = 0;

    virtual void prev() = 0;

    [[nodiscard]]
    virtual auto key() const -> std::string_view = 0;

    [[nodiscard]]
    virtual auto value() const -> std::string_view = 0;

    [[nodiscard]]
    virtual auto valid() const -> bool = 0;

    [[nodiscard]]
    virtual auto status() const -> Status = 0;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_ITERATOR_H
