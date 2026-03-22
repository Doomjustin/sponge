#include "sponge/utility.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using namespace spg;

TEST_CASE("numeric_cast parses valid integer", "[spg_base_utility][numeric_cast]")
{
    auto [value, ec] = numeric_cast<int>("123");

    REQUIRE_FALSE(ec);
    REQUIRE(value == 123);
}

TEST_CASE("numeric_cast reports invalid_argument", "[spg_base_utility][numeric_cast]")
{
    auto [value, ec] = numeric_cast<int>("abc");

    (void)value;
    REQUIRE(ec == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast reports out_of_range", "[spg_base_utility][numeric_cast]")
{
    auto [value, ec] = numeric_cast<std::int8_t>("200");

    (void)value;
    REQUIRE(ec == std::make_error_code(std::errc::result_out_of_range));
}

TEST_CASE("to_uppercase converts lowercase and keeps non-letters",
          "[spg_base_utility][to_uppercase]")
{
    auto result{ to_uppercase("aB1-?z") };

    REQUIRE(result == "AB1-?Z");
}

TEST_CASE("to_lowercase converts uppercase and keeps non-letters",
          "[spg_base_utility][to_lowercase]")
{
    auto result{ to_lowercase("Ab1-?Z") };

    REQUIRE(result == "ab1-?z");
}