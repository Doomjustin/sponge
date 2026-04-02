#include <sponge/http/templates.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::http;

namespace {

auto make_request(Method method, std::string_view path) -> Request
{
    return RequestBuilder{}.method(method).path(path).version(11).keep_alive(false).build();
}

} // namespace

TEST_CASE("templates::method_not_allowed 应格式化 method 名称", "[http][templates]")
{
    const auto request = make_request(Method::Post, "/ping");

    REQUIRE(templates::method_not_allowed(request) == "Method POST Not Allowed");
}

TEST_CASE("templates::error 应路由到专用错误消息", "[http][templates]")
{
    const auto request = make_request(Method::Get, "/missing");

    REQUIRE(templates::error(request, Status::NotFound) == "Resource /missing Not Found");
    REQUIRE(templates::error(request, Status::BadRequest) == "Bad Request: /missing");
}
