#ifndef SPONGE_TAG_H
#define SPONGE_TAG_H

#include <type_traits>

namespace spg {

template<typename T>
inline constexpr std::type_identity<T> as_type{};

struct UnlockTag {};
struct ReadOnlyTag {};
struct ReadWriteTag {};
struct ByRankTag {};
struct ExpandGreedyTag{};
struct ByHashTag {};
struct ToLastT {};
struct ToFirstT {};


inline constexpr UnlockTag unlock{};
inline constexpr ReadOnlyTag read_only{};
inline constexpr ReadWriteTag read_write{};
inline constexpr ByRankTag by_rank{};
inline constexpr ExpandGreedyTag greedy{};
inline constexpr ToFirstT to_first{};
inline constexpr ToLastT to_last{};

} // namespace spg

#endif // SPONGE_TAG_H
