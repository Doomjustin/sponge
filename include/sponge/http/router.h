#ifndef SPONGE_HTTP_ROUTER_H
#define SPONGE_HTTP_ROUTER_H

#include <expected>
#include <functional>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <ctre.hpp>
#include <glaze/glaze.hpp>

#include "invoke.h"
#include "method.h"
#include "middleware.h"
#include "request.h"
#include "response.h"
#include "status.h"
#include "templates.h"

namespace spg::http {

class Router {
public:
    using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;
    using Handler = std::function<std::expected<HttpResponse, Status>(const Request& request)>;
    using Middleware = middleware::Middleware;

    auto dispatch(const Request& request) -> std::expected<HttpResponse, Status>;

    void Use(Middleware middleware);

    template<Method method, ctll::fixed_string pattern, typename Func>
    void Map(Func&& func)
    {
        auto wrapper = [handler = std::forward<Func>(func)](const Request& request) 
            -> std::expected<HttpResponse, Status> 
        {
            if (auto match = ctre::match<pattern>(request.path())) {
                if constexpr (method != Method::Any) {
                    if ((request.method() & method) == Method::None)
                        return std::unexpected(Status::MethodNotAllowed);
                }

                try {
                    auto result = invoke_handler(handler, request, match);
                    return response_cast(std::move(result), request);
                }
                catch (const BadRequestException&) {
                    return std::unexpected(Status::BadRequest);
                }
            }

            return std::unexpected(Status::NotFound);
        };

        routes_.emplace_back(std::move(wrapper));
    }

private:
    template<typename T>
    struct is_expected_with_status : std::false_type {};

    template<typename V>
    struct is_expected_with_status<std::expected<V, Status>> : std::true_type {};

    template<typename T>
    static constexpr bool is_expected_with_status_v = is_expected_with_status<std::remove_cvref_t<T>>::value;

    std::vector<Handler> routes_;
    std::vector<Middleware> middlewares_;

    auto dispatch_routes(const Request& request) const -> std::expected<HttpResponse, Status>;

    template<typename Result>
    static auto response_cast(Result&& result, const Request& request) -> std::expected<HttpResponse, Status>
    {
        namespace http = boost::beast::http;

        if constexpr (is_expected_with_status_v<Result>) {
            if (!result)
                return std::unexpected(result.error());

            return response_cast(std::move(*result), request);
        }
        else {
            http::response<http::string_body> response{ http::status::ok, request.version() };
            response.keep_alive(request.keep_alive());

            if constexpr (std::is_convertible_v<Result, std::string>) {
                response.body() = std::forward<Result>(result);
                response.set(http::field::content_type, templates::CONTENT_TEXT);
            }
            else if constexpr (std::is_same_v<std::remove_cvref_t<Result>, Response>) {
                response.result(static_cast<http::status>(result.status()));
                response.body() = result.body();
                if (!result.content_type().empty())
                    response.set(http::field::content_type, result.content_type());

                for (auto&& [key, value] : result.headers())
                    response.insert(key, value);
            }
            else if constexpr (std::is_aggregate_v<std::remove_cvref_t<Result>>) {
                std::string json_buffer{};
                auto ec = glz::write_json(std::forward<Result>(result), json_buffer);

                if (!ec) {
                    response.body() = std::move(json_buffer);
                    response.set(http::field::content_type, templates::CONTENT_JSON);
                }
                else {
                    response.result(http::status::internal_server_error);
                    response.body() = "Failed to serialize JSON";
                    response.set(http::field::content_type, templates::CONTENT_TEXT);
                }
            }
            else {
                static_assert(false, "Unsupported handler return type");
            }

            response.prepare_payload();
            return response;
        }
    }
};

} // namespace spg::http

#endif // SPONGE_HTTP_ROUTER_H
