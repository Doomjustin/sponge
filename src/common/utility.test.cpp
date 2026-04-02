#include <sponge/utility.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("numeric_cast 在输入合法整数时应转换成功", "[common][utility]")
{
    auto res = numeric_cast<int>("123");

    REQUIRE(res);
    REQUIRE(*res == 123);
}

TEST_CASE("numeric_cast 在非法输入时应返回 invalid_argument", "[common][utility]")
{
    auto res = numeric_cast<int>("abc");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 在超出范围时应返回 out_of_range", "[common][utility]")
{
    auto res = numeric_cast<std::int8_t>("200");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::result_out_of_range));
}

TEST_CASE("numeric_cast 应正确转换负整数", "[common][utility]")
{
    auto res = numeric_cast<int>("-456");

    REQUIRE(res);
    REQUIRE(*res == -456);
}

TEST_CASE("numeric_cast 应正确转换零值", "[common][utility]")
{
    auto res = numeric_cast<int>("0");

    REQUIRE(res);
    REQUIRE(*res == 0);
}

TEST_CASE("numeric_cast 应正确转换大整数", "[common][utility]")
{
    auto res = numeric_cast<long long>("9223372036854775807");

    REQUIRE(res);
    REQUIRE(*res == 9223372036854775807LL);
}

TEST_CASE("numeric_cast 在无符号类型下应拒绝负整数", "[common][utility]")
{
    auto res = numeric_cast<unsigned int>("-1");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 在空字符串输入时应返回 invalid_argument", "[common][utility]")
{
    auto res = numeric_cast<int>("");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 在全空白字符串输入时应返回 invalid_argument", "[common][utility]")
{
    auto res = numeric_cast<int>("   ");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("numeric_cast 在部分有效数字输入时应返回 invalid_argument", "[common][utility]")
{
    // numeric_cast 要求整串完全匹配，因此 "123abc" 应视为非法。
    auto res = numeric_cast<int>("123abc");

    REQUIRE_FALSE(res);
    REQUIRE(res.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST_CASE("to_uppercase 应将小写字母转换为大写", "[common][utility]")
{
    auto result{ to_uppercase("aB1-?z") };

    REQUIRE(result == "AB1-?Z");
}

TEST_CASE("to_lowercase 应将大写字母转换为小写", "[common][utility]") {
    auto result{ to_lowercase("Ab1-?Z") };

    REQUIRE(result == "ab1-?z");
}

TEST_CASE("to_uppercase 在空字符串输入时应返回空字符串", "[common][utility]") {
    auto result{ to_uppercase("") };

    REQUIRE(result == "");
}

TEST_CASE("to_lowercase 在空字符串输入时应返回空字符串", "[common][utility]") {
    auto result{ to_lowercase("") };

    REQUIRE(result == "");
}

TEST_CASE("to_uppercase 应保持已有的大写字母", "[common][utility]") {
    auto result{ to_uppercase("HELLO WORLD") };

    REQUIRE(result == "HELLO WORLD");
}

TEST_CASE("to_lowercase 应保持已有的小写字母", "[common][utility]") {
    auto result{ to_lowercase("hello world") };

    REQUIRE(result == "hello world");
}

TEST_CASE("to_uppercase 应处理特殊字符和数字", "[common][utility]") {
    auto result{ to_uppercase("aBc!@#$%^&*()123XyZ") };

    REQUIRE(result == "ABC!@#$%^&*()123XYZ");
}

TEST_CASE("to_lowercase 应处理特殊字符和数字", "[common][utility]") {
    auto result{ to_lowercase("aBc!@#$%^&*()123XyZ") };

    REQUIRE(result == "abc!@#$%^&*()123xyz");
}

TEST_CASE("string_cast 应输出可被 numeric_cast 解析的浮点文本", "[common][utility]") {
    const double input = 3.141592653589793;
    auto text = string_cast(input);

    REQUIRE(text);
    REQUIRE(!text->empty());

    auto parsed = numeric_cast<double>(*text);
    REQUIRE(parsed);
    CHECK(std::abs(*parsed - input) <= std::numeric_limits<double>::epsilon() * std::abs(input));
}

TEST_CASE("std hash 对相同输入应返回稳定哈希值", "[common][utility]")
{
    REQUIRE(hash(use_std_hash("hello")) == hash(use_std_hash("hello")));
}

TEST_CASE("std hash 对不同输入应返回不同哈希值", "[common][utility]")
{
    REQUIRE(hash(use_std_hash("foo")) != hash(use_std_hash("bar")));
}

TEST_CASE("FNV-1a hash 对相同输入应返回稳定哈希值", "[common][utility]")
{
    REQUIRE(hash(use_fnv_1a("hello")) == hash(use_fnv_1a("hello")));
}

TEST_CASE("FNV-1a hash 应正确处理空字符串", "[common][utility]")
{
    const auto empty_hash = hash(use_fnv_1a(""));

    REQUIRE(empty_hash == hash(use_fnv_1a("")));
    REQUIRE(empty_hash != hash(use_fnv_1a("a")));
}

TEST_CASE("两种 hash 算法对同一输入都应保持各自结果稳定", "[common][utility]")
{
    const std::string test_strings[] = { "test", "hello", "world", "123", "" };

    for (const auto& text : test_strings) {
        const auto std_hash = hash(use_std_hash(text));
        const auto fnv_hash = hash(use_fnv_1a(text));

        REQUIRE(hash(use_std_hash(text)) == std_hash);
        REQUIRE(hash(use_fnv_1a(text)) == fnv_hash);
    }
}

TEST_CASE("StringHash 应支持 unordered_map 的异构查找", "[common][utility]")
{
    std::unordered_map<std::string, int, StringHash, std::equal_to<>> map{};
    map.emplace("key", 42);

    const std::string_view sv{ "key" };
    const auto it = map.find(sv);

    REQUIRE(it != map.end());
    REQUIRE(it->second == 42);
}

TEST_CASE("PmrStringHash 应支持 pmr::unordered_map 的异构查找", "[common][utility]")
{
    std::pmr::unordered_map<std::pmr::string, int, PmrStringHash, std::equal_to<>> map{};
    map.emplace("key", 99);

    const std::string_view sv{ "key" };
    const auto it = map.find(sv);

    REQUIRE(it != map.end());
    REQUIRE(it->second == 99);
}
