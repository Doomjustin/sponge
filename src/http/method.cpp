#include <sponge/http/method.h>

namespace spg::http {

auto format_as(Method method) -> std::string
{
    switch (method) {
    case Method::None:
        return "NONE";
    case Method::Get:
        return "GET";
    case Method::Post:
        return "POST";
    case Method::Put:
        return "PUT";
    case Method::Delete:
        return "DELETE";
    case Method::Patch:
        return "PATCH";
    case Method::Options:
        return "OPTIONS";
    default:
        return "UNKNOWN";
    }
}

} // namespace spg::http
