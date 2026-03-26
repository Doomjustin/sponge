#ifndef SPONGE_HTTP_METHOD_H
#define SPONGE_HTTP_METHOD_H

#include <cstdint>
#include <string>

namespace spg::http {

enum class Method : uint8_t {
    None = 0,
    Get = 1 << 0,
    Post = 1 << 1,
    Put = 1 << 2,
    Delete = 1 << 3,
    Patch = 1 << 4,
    Options = 1 << 5,
    Any = 0xFF,
};

constexpr auto operator|(Method lhs, Method rhs) -> Method
{
    return static_cast<Method>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr auto operator&(Method lhs, Method rhs) -> Method
{
    return static_cast<Method>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

auto format_as(Method method) -> std::string;

} // namespace spg::http

#endif // SPONGE_HTTP_METHOD_H
