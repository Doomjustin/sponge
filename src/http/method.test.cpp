#include <sponge/http/method.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::http;

TEST_CASE("Method 枚举值应保持稳定", "[http][method]")
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

TEST_CASE("Method 按位或应组合标志位", "[http][method]")
{
    auto methods{ Method::Get | Method::Post | Method::Patch };

    REQUIRE(methods == static_cast<Method>((1 << 0) | (1 << 1) | (1 << 4)));
}

TEST_CASE("Method 按位与应保留公共标志位", "[http][method]")
{
    auto methods{ (Method::Get | Method::Post | Method::Patch) & (Method::Post | Method::Patch) };

    REQUIRE(methods == (Method::Post | Method::Patch));
}

TEST_CASE("Method::any 应匹配所有已声明标志位", "[http][method]")
{
    auto declared_methods{ Method::Get | Method::Post | Method::Put | Method::Delete |
                           Method::Patch | Method::Options };

    REQUIRE((Method::Any & declared_methods) == declared_methods);
    REQUIRE((Method::None & Method::Get) == Method::None);
}

