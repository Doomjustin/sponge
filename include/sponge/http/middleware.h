#ifndef SPONGE_HTTP_MIDDLEWARE_H
#define SPONGE_HTTP_MIDDLEWARE_H

#include <expected>
#include <functional>
#include <string_view>

#include <boost/beast.hpp>

#include "request.h"
#include "status.h"

namespace spg::http {

struct middleware {
    middleware() = delete;
    using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;
    using DispatchResult = std::expected<HttpResponse, Status>;
    using Next = std::function<std::expected<HttpResponse, Status>(const Request& request)>;
    using Middleware = std::function<std::expected<HttpResponse, Status>(const Request& request, const Next& next)>;

    static auto request_id(std::string_view header_name = "x-request-id") -> Middleware;

    static auto access_log(std::string_view request_id_header = "x-request-id") -> Middleware;

    static auto recover() -> Middleware;
};

} // namespace spg::http

#endif // SPONGE_HTTP_MIDDLEWARE_H
