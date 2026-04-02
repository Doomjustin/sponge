#ifndef SPONGE_FORMAT_H
#define SPONGE_FORMAT_H

#include <concepts>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/ranges.h>
#include <fmt/std.h>

namespace spg {

template <typename T>
concept has_to_string = requires(T& t) 
{
    { t.to_string() } -> std::convertible_to<std::string>;
};

template <typename T>
concept has_to_repr = requires(T& t) 
{
    { t.to_repr() } -> std::convertible_to<std::string>;
};

template <typename T>
concept has_output_operator = requires(T& t, std::ostream& os)
{
    { os << t } -> std::same_as<std::ostream&>;
};

template <typename T>
    requires has_to_string<T>
auto stringify_for_format(const T& t) -> std::string
{
    return t.to_string();
}

template <typename T>
    requires(!has_to_string<T>) && has_to_repr<T>
auto stringify_for_format(const T& t) -> std::string
{
    return t.to_repr();
}

template <typename T>
    requires(!has_to_string<T>) && (!has_to_repr<T>) && has_output_operator<T>
auto stringify_for_format(const T& t) -> std::string
{
    std::ostringstream os;
    os << t;
    return os.str();
}

template <typename T>
    requires(!has_to_string<T>) && (!has_to_repr<T>) && (!has_output_operator<T>)
auto stringify_for_format(const T& t) -> std::string
{
    return typeid(T).name() + std::string{ "@" } + fmt::format("{:p}", static_cast<const void*>(&t));
}

template <typename... Args>
auto xformat(const std::string_view pattern, Args&&... args) -> std::string
{
    auto make_arg = []<typename T>(T&& value) -> decltype(auto) {
        using raw_type = std::remove_cvref_t<T>;
        if constexpr (fmt::is_formattable<raw_type, char>::value)
            return std::forward<T>(value);
        else
            return stringify_for_format(value);
    };

    return fmt::format(fmt::runtime(pattern), make_arg(std::forward<Args>(args))...);
}

} // namespace spg

#endif // SPONGE_FORMAT_H
