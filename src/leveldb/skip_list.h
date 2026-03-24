#ifndef SPONGE_LEVELDB_SKIP_LIST_H
#define SPONGE_LEVELDB_SKIP_LIST_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <memory_resource>

#include <sponge/random.h>

namespace spg::leveldb {

// comparator(lhs, rhs) < 0 if lhs < rhs
// comparator(lhs, rhs) == 0 if lhs == rhs
// comparator(lhs, rhs) > 0 if lhs > rhs
template<typename T, typename V>
concept SkipListComparator = requires(T t, V lhs, V rhs) {
    { t(lhs, rhs) } -> std::convertible_to<int>;
};

// TODO: support custom allocator

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_SKIP_LIST_H
