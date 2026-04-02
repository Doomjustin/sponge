#include <sponge/fixed_string.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("FixedString 使用字符串字面量构造时应完整复制字符与结尾空字符", "[common][fixed_string]")
{
	constexpr FixedString text{ "xin" };

	static_assert(text.data[0] == 'x');
	static_assert(text.data[1] == 'i');
	static_assert(text.data[2] == 'n');
	static_assert(text.data[3] == '\0');

	REQUIRE(text.data[0] == 'x');
	REQUIRE(text.data[1] == 'i');
	REQUIRE(text.data[2] == 'n');
	REQUIRE(text.data[3] == '\0');
}

TEST_CASE("FixedString::view() 应返回不包含结尾空字符的字符串视图", "[common][fixed_string]")
{
	constexpr FixedString text{ "fixed" };

	static_assert(text.view() == "fixed");
	static_assert(text.view().size() == 5);

	REQUIRE(text.view() == "fixed");
	REQUIRE(text.view().size() == 5);
}

TEST_CASE("FixedString 比较相同内容时应判定为相等", "[common][fixed_string]")
{
	constexpr FixedString lhs{ "same" };
	constexpr FixedString rhs{ "same" };

	static_assert(lhs == rhs);

	REQUIRE(lhs == rhs);
}

TEST_CASE("FixedString 比较不同内容时应按字典序比较大小", "[common][fixed_string]")
{
	constexpr FixedString lhs{ "able" };
	constexpr FixedString rhs{ "bake" };

	static_assert(lhs < rhs);

	REQUIRE(lhs < rhs);
}

TEST_CASE("多个 FixedString 排序后应按字典序排列", "[common][fixed_string]")
{
	constexpr std::array source{ FixedString{"ccc"}, FixedString{"aaa"}, FixedString{"bbb"} };
	auto values{ source };

	std::ranges::sort(values);

	REQUIRE(values[0].view() == "aaa");
	REQUIRE(values[1].view() == "bbb");
	REQUIRE(values[2].view() == "ccc");
}
