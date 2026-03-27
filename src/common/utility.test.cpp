#include <sponge/utility.h>

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("numeric_cast parses valid integer", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<int>("123");

    REQUIRE(res);
    REQUIRE(*res == 123);
}

TEST_CASE("numeric_cast reports invalid_argument", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<int>("abc");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast reports out_of_range", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<std::int8_t>("200");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::result_out_of_range));
}

TEST_CASE("numeric_cast handles negative integers", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<int>("-456");

    REQUIRE(res);
    REQUIRE(*res == -456);
}

TEST_CASE("numeric_cast handles zero", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<int>("0");

    REQUIRE(res);
    REQUIRE(*res == 0);
}

TEST_CASE("numeric_cast handles large numbers", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<long long>("9223372036854775807");

    REQUIRE(res);
    REQUIRE(*res == 9223372036854775807LL);
}

TEST_CASE("numeric_cast with unsigned type rejects negative", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<unsigned int>("-1");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast handles empty string", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<int>("");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast handles whitespace-only string", "[spg_base_utility][numeric_cast]")
{
    auto res = numeric_cast<int>("   ");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast with partial valid number succeeds with leading digits",
          "[spg_base_utility][numeric_cast]")
{
    // from_chars parses leading valid digits in "123abc"
    auto res = numeric_cast<int>("123abc");

    REQUIRE(res);
    REQUIRE(*res == 123);
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

TEST_CASE("to_uppercase with empty string returns empty", "[spg_base_utility][to_uppercase]")
{
    auto result{ to_uppercase("") };

    REQUIRE(result == "");
}

TEST_CASE("to_lowercase with empty string returns empty", "[spg_base_utility][to_lowercase]")
{
    auto result{ to_lowercase("") };

    REQUIRE(result == "");
}

TEST_CASE("to_uppercase preserves already uppercase", "[spg_base_utility][to_uppercase]")
{
    auto result{ to_uppercase("HELLO WORLD") };

    REQUIRE(result == "HELLO WORLD");
}

TEST_CASE("to_lowercase preserves already lowercase", "[spg_base_utility][to_lowercase]")
{
    auto result{ to_lowercase("hello world") };

    REQUIRE(result == "hello world");
}

TEST_CASE("to_uppercase handles special characters and numbers",
          "[spg_base_utility][to_uppercase]")
{
    auto result{ to_uppercase("aBc!@#$%^&*()123XyZ") };

    REQUIRE(result == "ABC!@#$%^&*()123XYZ");
}

TEST_CASE("to_lowercase handles special characters and numbers",
          "[spg_base_utility][to_lowercase]")
{
    auto result{ to_lowercase("aBc!@#$%^&*()123XyZ") };

    REQUIRE(result == "abc!@#$%^&*()123xyz");
}