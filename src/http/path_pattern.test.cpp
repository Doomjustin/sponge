#include <sponge/http/path_pattern.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::http;

TEST_CASE("Path pattern converts str segments", "[spg_http_path_pattern][str]")
{
    constexpr auto regex = path_pattern_to_regex<"/ping/<str>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/ping/([^/]+)" }));
}

TEST_CASE("Path pattern ignores parameter names", "[spg_http_path_pattern][named]")
{
    constexpr auto regex = path_pattern_to_regex<"/users/<int:id>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/users/(-?[0-9]+)" }));
}

TEST_CASE("Path pattern converts float segments", "[spg_http_path_pattern][float]")
{
    constexpr auto regex = path_pattern_to_regex<"/prices/<float:value>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/prices/(-?[0-9]+(?:\\.[0-9]+)?)" }));
}

TEST_CASE("Path pattern escapes literal regex meta characters", "[spg_http_path_pattern][escape]")
{
    constexpr auto regex = path_pattern_to_regex<"/files.v1/<slug>">();

    REQUIRE(regex.is_same_as(ctll::fixed_string{ "/files\\.v1/([A-Za-z0-9_-]+)" }));
}