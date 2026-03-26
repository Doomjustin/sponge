#include <sponge/http/method.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::http;

TEST_CASE("Method enum values are stable", "[spg_http_method][enum]")
{
    REQUIRE(Method::None == static_cast<Method>(0));
    REQUIRE(Method::Get == static_cast<Method>(1 << 0));
    REQUIRE(Method::Post == static_cast<Method>(1 << 1));
    REQUIRE(Method::Put == static_cast<Method>(1 << 2));
    REQUIRE(Method::Delete == static_cast<Method>(1 << 3));
    REQUIRE(Method::Patch == static_cast<Method>(1 << 4));
    REQUIRE(Method::Options == static_cast<Method>(1 << 5));
    REQUIRE(Method::Any == static_cast<Method>(0xFF));
}

TEST_CASE("Method bitwise or combines flags", "[spg_http_method][bitwise]")
{
    auto methods{ Method::Get | Method::Post | Method::Patch };

    REQUIRE(methods == static_cast<Method>((1 << 0) | (1 << 1) | (1 << 4)));
}

TEST_CASE("Method bitwise and keeps shared flags", "[spg_http_method][bitwise]")
{
    auto methods{ (Method::Get | Method::Post | Method::Patch) & (Method::Post | Method::Patch) };

    REQUIRE(methods == (Method::Post | Method::Patch));
}

TEST_CASE("Method any matches all declared flags", "[spg_http_method][bitwise]")
{
    auto declared_methods{ Method::Get | Method::Post | Method::Put | Method::Delete |
                           Method::Patch | Method::Options };

    REQUIRE((Method::Any & declared_methods) == declared_methods);
    REQUIRE((Method::None & Method::Get) == Method::None);
}

