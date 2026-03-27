#ifndef SPONGE_UTILITY_H
#define SPONGE_UTILITY_H

#include <charconv>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

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

auto to_uppercase(std::string_view input) -> std::string;

auto to_lowercase(std::string_view input) -> std::string;

} // namespace spg

#endif // SPONGE_UTILITY_H
