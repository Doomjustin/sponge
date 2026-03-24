#ifndef SPONGE_LEVELDB_COMPARATOR_H
#define SPONGE_LEVELDB_COMPARATOR_H

#include <cstddef>
#include <span>

namespace spg::leveldb {

struct Comparator {
    using ByteView = std::span<const std::byte>;

    virtual ~Comparator() = default;

    [[nodiscard]]
    virtual auto compare(ByteView a, ByteView b) const -> int = 0;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_COMPARATOR_H
