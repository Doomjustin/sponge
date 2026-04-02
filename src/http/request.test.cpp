#include <sponge/http/request.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::http;

TEST_CASE("RequestBuilder 应保存请求核心属性", "[http][request]")
{
    auto request = RequestBuilder{}
                       .method(Method::Post)
                       .path("/users")
                       .body("payload")
                       .version(11)
                       .keep_alive(true)
                       .build();

    REQUIRE(request.method() == Method::Post);
    REQUIRE(request.path() == "/users");
    REQUIRE(request.body() == "payload");
    REQUIRE(request.version() == 11);
    REQUIRE(request.keep_alive());
}

TEST_CASE("RequestBuilder 应通过 get_header 暴露 headers", "[http][request]")
{
    auto request = RequestBuilder{}
                       .add_header("content-type", "application/json")
                       .add_header("x-request-id", "42")
                       .build();

    REQUIRE(request.get_header("content-type") == "application/json");
    REQUIRE(request.get_header("x-request-id") == "42");
    REQUIRE_FALSE(request.get_header("missing").has_value());
}
