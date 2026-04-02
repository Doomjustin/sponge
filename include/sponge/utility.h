#ifndef SPONGE_UTILITY_H
#define SPONGE_UTILITY_H

#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace spg {

template<typename T>
auto numeric_cast(std::string_view str) -> std::expected<T, std::error_code>
{
    T value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

    if (ec != std::errc{})
        return std::unexpected(std::make_error_code(ec));

    if (ptr != str.data() + str.size())
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));

    return value;
}

template<std::floating_point T>
auto string_cast(T value) -> std::expected<std::string, std::error_code>
{
    // 符号/小数点/指数("e+NNN") 余量
    constexpr size_t buffer_size = static_cast<size_t>(std::numeric_limits<T>::max_digits10) + 8;

    std::array<char, buffer_size> buffer{};

    auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (ec != std::errc{})
        return std::unexpected(std::make_error_code(ec));

    if (ptr == buffer.data())
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));

    return std::string(buffer.data(), static_cast<size_t>(ptr - buffer.data()));
}

auto to_uppercase(std::string_view input) -> std::string;

auto to_lowercase(std::string_view input) -> std::string;

template<typename T, template <typename...> class Template>
struct is_specialization_of: std::false_type {};

template<template <typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template>: std::true_type {};

template<template <typename...> class Template, typename... Args>
inline constexpr bool is_specialization_of_v = is_specialization_of<Args..., Template>::value;


struct UseStdHashT {
    std::string_view value;
};

struct UseFNV1aHashT {
    std::string_view value;
};

constexpr auto use_std_hash(const std::string_view value) -> UseStdHashT
{
    return UseStdHashT{ value };
}

constexpr auto use_fnv_1a(const std::string_view value) -> UseFNV1aHashT
{
    return UseFNV1aHashT{ value };
}

[[nodiscard]]
inline auto hash(const UseStdHashT value) -> size_t
{
    using Hasher = std::hash<std::string_view>;
    return Hasher{}(value.value);
}

[[nodiscard]]
constexpr auto hash(const UseFNV1aHashT value) -> uint64_t
{
    constexpr uint64_t FNV_offset_basis = 14695981039346656037ULL;
    constexpr uint64_t FNV_prime = 1099511628211ULL;

    uint64_t result = FNV_offset_basis;
    for (const unsigned char byte : value.value) {
        result ^= byte;
        result *= FNV_prime;
    }

    return result;
}


// 支持hash表的异构查找
struct StringHash {
    using is_transparent = void;

    auto operator()(const std::string_view sv) const noexcept -> size_t
    {
        return static_cast<size_t>(hash(use_fnv_1a(sv)));
    }
};

// 支持hash表的异构查找
struct PmrStringHash {
    using is_transparent = void;

    auto operator()(const std::string_view sv) const -> size_t
    {
        return static_cast<size_t>(hash(use_fnv_1a(sv)));
    }
};

} // namespace spg

#endif // SPONGE_UTILITY_H
