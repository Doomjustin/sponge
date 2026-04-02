#include <sponge/utility.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("numeric_cast 应转换有效的整数", "[utility]")
{
    auto res = numeric_cast<int>("123");

    REQUIRE(res);
    REQUIRE(*res == 123);
}

TEST_CASE("numeric_cast 应报告 invalid_argument", "[utility]")
{
    auto res = numeric_cast<int>("abc");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 应报告 out_of_range", "[utility]")
{
    auto res = numeric_cast<std::int8_t>("200");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::result_out_of_range));
}

TEST_CASE("numeric_cast 应处理负整数", "[utility]")
{
    auto res = numeric_cast<int>("-456");

    REQUIRE(res);
    REQUIRE(*res == -456);
}

TEST_CASE("numeric_cast 应处理零", "[utility]")
{
    auto res = numeric_cast<int>("0");

    REQUIRE(res);
    REQUIRE(*res == 0);
}

TEST_CASE("numeric_cast 应处理大整数", "[utility]")
{
    auto res = numeric_cast<long long>("9223372036854775807");

    REQUIRE(res);
    REQUIRE(*res == 9223372036854775807LL);
}

TEST_CASE("numeric_cast 对于无符号整数类型应拒绝负整数", "[utility]")
{
    auto res = numeric_cast<unsigned int>("-1");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 应处理空字符串", "[utility]")
{
    auto res = numeric_cast<int>("");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 应处理仅空格字符串", "[utility]")
{
    auto res = numeric_cast<int>("   ");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 应报告部分有效数字的无效", "[utility]")
{
    // numeric_cast 要求整串完全匹配，因此 "123abc" 应视为非法。
    auto res = numeric_cast<int>("123abc");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("to_uppercase 应将小写字母改为大写并不改下于性文字", "[utility]")
{
    auto result{ to_uppercase("aB1-?z") };

    REQUIRE(result == "AB1-?Z");
}

TEST_CASE("to_lowercase 应佰改大写字母并不加处理靠字符", "[utility]") {
    auto result{ to_lowercase("Ab1-?Z") };

    REQUIRE(result == "ab1-?z");
}

TEST_CASE("to_uppercase 对于空字符串应返回空", "[utility]") {
    auto result{ to_uppercase("") };

    REQUIRE(result == "");
}

TEST_CASE("to_lowercase 对于空字符串应返回空", "[utility]") {
    auto result{ to_lowercase("") };

    REQUIRE(result == "");
}

TEST_CASE("to_uppercase 应保持已有的大写字母", "[utility]") {
    auto result{ to_uppercase("HELLO WORLD") };

    REQUIRE(result == "HELLO WORLD");
}

TEST_CASE("to_lowercase 应保持已有的小写字母", "[utility]") {
    auto result{ to_lowercase("hello world") };

    REQUIRE(result == "hello world");
}

TEST_CASE("to_uppercase 应处理特殊字符和数字", "[utility]") {
    auto result{ to_uppercase("aBc!@#$%^&*()123XyZ") };

    REQUIRE(result == "ABC!@#$%^&*()123XYZ");
}

TEST_CASE("to_lowercase 应处理特殊字符和数字", "[utility]") {
    auto result{ to_lowercase("aBc!@#$%^&*()123XyZ") };

    REQUIRE(result == "abc!@#$%^&*()123xyz");
}

TEST_CASE("string_cast 应转换浮点数并能解析回来", "[utility]") {
    const double input = 3.141592653589793;
    auto text = string_cast(input);

    REQUIRE(text);
    REQUIRE(!text->empty());

    auto parsed = numeric_cast<double>(*text);
    REQUIRE(parsed);
    CHECK(std::abs(*parsed - input) <= std::numeric_limits<double>::epsilon() * std::abs(input));
}