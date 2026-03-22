#ifndef SPONGE_UTILITY_H
#define SPONGE_UTILITY_H

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

namespace spg {

template<typename T>
auto numeric_cast(std::string_view str) -> std::pair<T, std::error_code>
{
    T value;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    return { value, std::make_error_code(ec) };
}

auto to_uppercase(std::string_view input) -> std::string;

auto to_lowercase(std::string_view input) -> std::string;

} // namespace spg

#endif // SPONGE_UTILITY_H
