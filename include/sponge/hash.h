#ifndef SPONGE_HASH_H
#define SPONGE_HASH_H

#include <cstddef>
#include <string>
#include <string_view>

namespace spg {

struct UseStdTag {};
struct UseFNV1aTag {};

static constexpr UseStdTag use_std{};
static constexpr UseFNV1aTag use_fnv_1a{};

[[nodiscard]]
auto hash(std::string_view key, UseStdTag tag) -> size_t;

[[nodiscard]]
auto hash(std::string_view key, UseFNV1aTag tag) -> size_t;

// 支持hash表的异构查找
struct StringHash {
    using is_transparent = void;

    auto operator()(std::string_view sv) const noexcept -> size_t { return hash(sv, use_fnv_1a); }

    auto operator()(const std::string& s) const noexcept -> size_t { return hash(s, use_fnv_1a); }
};

// 支持hash表的异构查找
struct PmrStringHash {
    using is_transparent = void;

    auto operator()(std::string_view sv) const -> size_t { return hash(sv, use_fnv_1a); }

    auto operator()(const std::pmr::string& s) const -> size_t
    {
        return hash(std::string_view(s.data(), s.size()), use_fnv_1a);
    }
};

} // namespace spg

#endif // SPONGE_HASH_H
