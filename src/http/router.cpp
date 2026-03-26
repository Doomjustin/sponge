#include <sponge/http/router.h>

#include <ranges>

namespace spg::http {

void Router::Use(Middleware middleware) { middlewares_.emplace_back(std::move(middleware)); }

auto Router::dispatch_routes(const Request& request) const -> std::expected<HttpResponse, Status>
{
    auto saw_method_not_allowed = false;

    for (const auto& route : routes_) {
        auto current = route(request);

        if (current)
            return current;

        if (current.error() == Status::MethodNotAllowed) {
            saw_method_not_allowed = true;
            continue;
        }

        if (current.error() != Status::NotFound)
            return std::unexpected(current.error());
    }

    if (saw_method_not_allowed)
        return std::unexpected(Status::MethodNotAllowed);

    return std::unexpected(Status::NotFound);
}

auto Router::dispatch(const Request& request) -> std::expected<HttpResponse, Status>
{
    middleware::Next next = [this](const Request& req) { return dispatch_routes(req); };

    for (auto middleware : std::views::reverse(middlewares_)) {
        auto current_next = std::move(next);

        next = [middleware = std::move(middleware), inner_next = std::move(current_next)](const Request& req)
            -> std::expected<HttpResponse, Status>
        { 
            return middleware(req, inner_next); 
        };
    }

    return next(request);
}

} // namespace spg::http
