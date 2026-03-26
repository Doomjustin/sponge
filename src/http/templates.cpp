#include <sponge/http/templates.h>

#include <fmt/format.h>

namespace spg::http {

auto templates::method_not_allowed(const Request& request) -> std::string
{
    return fmt::format("Method {} Not Allowed", request.method());
}

auto templates::not_found(const Request& request) -> std::string
{
    return fmt::format("Resource {} Not Found", request.path());
}

auto templates::error(const Request& request, Status status) -> std::string
{
    switch (status) {
    case Status::BadRequest:
        return fmt::format("Bad Request: {}", request.path());
    case Status::InternalServerError:
        return fmt::format("Internal Server Error: {}", request.path());
    case Status::NotFound:
        return not_found(request);
    case Status::MethodNotAllowed:
        return method_not_allowed(request);
    default:
        return fmt::format("Error {}: {}", static_cast<int>(status), request.path());
    }
}

} // namespace spg::http
