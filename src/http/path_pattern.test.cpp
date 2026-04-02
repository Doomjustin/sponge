#include <sponge/http/path_pattern.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::http;

TEST_CASE("Path pattern 应将 str 段转换为匹配规则", "[http][path_pattern]")
{
    constexpr auto regex = path_pattern_to_regex<"/ping/<str>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/ping/([^/]+)" }));
}

TEST_CASE("Path pattern 应忽略参数名差异", "[http][path_pattern]")
{
    constexpr auto regex = path_pattern_to_regex<"/users/<int:id>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/users/(-?[0-9]+)" }));
}

TEST_CASE("Path pattern 应将 float 段转换为匹配规则", "[http][path_pattern]")
{
    constexpr auto regex = path_pattern_to_regex<"/prices/<float:value>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/prices/(-?[0-9]+(?:\\.[0-9]+)?)" }));
}

TEST_CASE("Path pattern 应转义字面量 regex 元字符", "[http][path_pattern]")
{
    constexpr auto regex = path_pattern_to_regex<"/files.v1/<slug>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/files\\.v1/([A-Za-z0-9_-]+)" }));
}
