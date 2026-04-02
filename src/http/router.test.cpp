#include <sponge/http/router.h>

#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sponge/http/middleware.h>

using namespace spg::http;

namespace {

auto make_request(Method method, std::string_view path) -> Request
{
    return RequestBuilder{}.method(method).path(path).version(11).keep_alive(false).build();
}

auto pong(const Request& request) -> std::string { return "pong"; }

auto echo_float(const Request& request, float value) -> std::string
{
    return std::to_string(value);
}

} // namespace

TEST_CASE("Router 应分发到匹配路由", "[http][router]")
{
    Router router{};
    router.Map<Method::Get, "/ping">(pong);

    auto result = router.dispatch(make_request(Method::Get, "/ping"));

    REQUIRE(result.has_value());
    REQUIRE(result->result_int() == static_cast<int>(Status::Ok));
    REQUIRE(result->body() == "pong");
}

TEST_CASE("Router 在前序方法不匹配时应继续匹配后续成功路由",
          "[http][router]")
{
    Router router{};
    router.Map<Method::Post, "/ping">(pong);
    router.Map<Method::Get, "/ping">(pong);

    auto result = router.dispatch(make_request(Method::Get, "/ping"));

    REQUIRE(result.has_value());
    REQUIRE(result->result_int() == static_cast<int>(Status::Ok));
    REQUIRE(result->body() == "pong");
}

TEST_CASE("Router 在路径匹配但方法不匹配时应返回 method not allowed",
          "[http][router]")
{
    Router router{};
    router.Map<Method::Post, "/ping">(pong);

    auto result = router.dispatch(make_request(Method::Get, "/ping"));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Status::MethodNotAllowed);
}

TEST_CASE("Router 应按位置注入 float 路径参数", "[http][router]")
{
    Router router{};
    router.Map<Method::Get, "/price/(-?[0-9]+(?:\\.[0-9]+)?)">(echo_float);

    auto result = router.dispatch(make_request(Method::Get, "/price/3.5"));

    REQUIRE(result.has_value());
    REQUIRE(result->body() == "3.500000");
}

TEST_CASE("Router middleware 应按顺序包装 handler", "[http][router]")
{
    Router router{};
    std::vector<std::string> order{};

    router.Use(
        [&](const Request& request, const middleware::Next& next) -> middleware::DispatchResult {
            order.emplace_back("mw-before");
            auto result = next(request);
            order.emplace_back("mw-after");
            return result;
        });

    router.Map<Method::Get, "/ping">([&](const Request& request) -> std::string {
        order.emplace_back("handler");
        return "pong";
    });

    auto result = router.dispatch(make_request(Method::Get, "/ping"));

    REQUIRE(result.has_value());
    REQUIRE(order == std::vector<std::string>{ "mw-before", "handler", "mw-after" });
}

TEST_CASE("Router middleware 应支持 short-circuit", "[http][router]")
{
    Router router{};
    auto handler_called = false;

    router.Use(
        [&](const Request& request, const middleware::Next& next) -> middleware::DispatchResult {
            return std::unexpected(Status::Unauthorized);
        });

    router.Map<Method::Get, "/ping">([&](const Request& request) -> std::string {
        handler_called = true;
        return "pong";
    });

    auto result = router.dispatch(make_request(Method::Get, "/ping"));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Status::Unauthorized);
    REQUIRE_FALSE(handler_called);
}

TEST_CASE("Router 搭配 recover middleware 时应将异常转换为 internal server error",
          "[http][router]")
{
    Router router{};
    router.Use(middleware::recover());

    router.Map<Method::Get, "/boom">(
        [](const Request& request) -> std::string { throw std::runtime_error("boom"); });

    auto result = router.dispatch(make_request(Method::Get, "/boom"));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Status::InternalServerError);
}

TEST_CASE("Router 应将 middleware 抛出的 BadRequestException 转换为 bad request",
          "[http][router]")
{
    Router router{};
    router.Use(middleware::recover());

    router.Use(
        [](const Request& request, const middleware::Next& next) -> middleware::DispatchResult {
            throw BadRequestException{ "bad body" };
        });

    router.Map<Method::Get, "/boom">(pong);

    auto result = router.dispatch(make_request(Method::Get, "/boom"));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Status::BadRequest);
}

TEST_CASE("Router 应支持 handler 返回 expected 成功值", "[http][router]")
{
    Router router{};

    router.Map<Method::Get, "/expected-ok">(
        [](const Request& request) -> std::expected<std::string, Status> { return "ok"; });

    auto result = router.dispatch(make_request(Method::Get, "/expected-ok"));

    REQUIRE(result.has_value());
    REQUIRE(result->result_int() == static_cast<int>(Status::Ok));
    REQUIRE(result->body() == "ok");
}

TEST_CASE("Router 应支持 handler 返回 expected 错误值", "[http][router]")
{
    Router router{};

    router.Map<Method::Get, "/expected-err">(
        [](const Request& request) -> std::expected<std::string, Status> {
            return std::unexpected(Status::Forbidden);
        });

    auto result = router.dispatch(make_request(Method::Get, "/expected-err"));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Status::Forbidden);
}

TEST_CASE("Router 应支持带自定义 headers 的 Response", "[http][router]")
{
    Router router{};

    router.Map<Method::Get, "/response">([](const Request& request) -> Response {
        Response response{};
        response.status(Status::Created)
            .body("created")
            .content_type("application/json")
            .set_header("x-request-id", "rid-123")
            .set_header("location", "/resource/1");
        return response;
    });

    auto result = router.dispatch(make_request(Method::Get, "/response"));

    REQUIRE(result.has_value());
    REQUIRE(result->result_int() == static_cast<int>(Status::Created));
    REQUIRE(result->body() == "created");
    REQUIRE(result->at(boost::beast::http::field::content_type) == "application/json");
    REQUIRE(result->at("x-request-id") == "rid-123");
    REQUIRE(result->at("location") == "/resource/1");
}

TEST_CASE("Router 应支持 expected<Response, Status>", "[http][router]")
{
    Router router{};

    router.Map<Method::Get, "/expected-response">(
        [](const Request& request) -> std::expected<Response, Status> {
            Response response{};
            response.status(Status::Accepted).body("accepted");
            return response;
        });

    auto result = router.dispatch(make_request(Method::Get, "/expected-response"));

    REQUIRE(result.has_value());
    REQUIRE(result->result_int() == static_cast<int>(Status::Accepted));
    REQUIRE(result->body() == "accepted");
}

TEST_CASE("Router 应支持重复 response headers", "[http][router]")
{
    Router router{};

    router.Map<Method::Get, "/cookies">([](const Request& request) -> Response {
        Response response{};
        response.status(Status::Ok)
            .body("ok")
            .set_cookie("a=1; Path=/")
            .set_cookie("b=2; Path=/")
            .add_header("x-extra", "v1")
            .add_header("x-extra", "v2");
        return response;
    });

    auto result = router.dispatch(make_request(Method::Get, "/cookies"));

    REQUIRE(result.has_value());
    REQUIRE(result->count("set-cookie") == 2);
    REQUIRE(result->count("x-extra") == 2);
}

TEST_CASE("Request ID middleware 应生成 response request id", "[http][router]")
{
    Router router{};
    router.Use(middleware::request_id());
    router.Map<Method::Get, "/rid">(pong);

    auto result = router.dispatch(make_request(Method::Get, "/rid"));

    REQUIRE(result.has_value());
    auto it = result->find("x-request-id");
    REQUIRE(it != result->end());
    REQUIRE_FALSE(std::string{ it->value() }.empty());
}

TEST_CASE("Request ID middleware 应透传传入的 request id", "[http][router]")
{
    Router router{};
    router.Use(middleware::request_id());
    router.Map<Method::Get, "/rid">(pong);

    RequestBuilder builder{};
    builder.method(Method::Get)
        .path("/rid")
        .version(11)
        .keep_alive(false)
        .add_header("x-request-id", "rid-incoming");

    auto result = router.dispatch(builder.build());

    REQUIRE(result.has_value());
    REQUIRE(result->at("x-request-id") == "rid-incoming");
}

TEST_CASE("Recover middleware 应将抛出异常映射为 internal server error",
          "[http][router]")
{
    Router router{};
    router.Use(middleware::recover());
    router.Map<Method::Get, "/boom">(
        [](const Request& request) -> std::string { throw std::runtime_error("boom"); });

    auto result = router.dispatch(make_request(Method::Get, "/boom"));

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Status::InternalServerError);
}
