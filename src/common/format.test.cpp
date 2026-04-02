#include <sponge/format.h>

#include <catch2/catch_test_macros.hpp>

namespace {

struct WithToString {
	auto to_string() const -> std::string { return "to_string_value"; }
};

struct WithToRepr {
	auto to_repr() const -> std::string { return "to_repr_value"; }
};

struct WithOutputOperator {
	int value{0};
};

auto operator<<(std::ostream& os, const WithOutputOperator& obj) -> std::ostream&
{
	return os << "ostream_value=" << obj.value;
}

struct WithoutStringifySupport {
	int value{0};
};

} // namespace

TEST_CASE("xformat() 传入基本类型参数时应按模板输出", "[common][format]")
{
	const auto text = spg::xformat("name={} age={}", "spg", 18);

	REQUIRE(text == "name=spg age=18");
}

TEST_CASE("xformat() 参数提供 to_string 时应优先使用其结果", "[common][format]")
{
	const WithToString value{};
	const auto text = spg::xformat("{}", value);

	REQUIRE(text == "to_string_value");
}

TEST_CASE("xformat() 参数提供 to_repr 但无 to_string 时应使用 to_repr 结果", "[common][format]")
{
	const WithToRepr value{};
	const auto text = spg::xformat("{}", value);

	REQUIRE(text == "to_repr_value");
}

TEST_CASE("xformat() 参数仅支持输出运算符时应使用流输出结果", "[common][format]")
{
	const WithOutputOperator value{42};
	const auto text = spg::xformat("{}", value);

	REQUIRE(text == "ostream_value=42");
}

TEST_CASE("xformat() 参数无字符串化能力时应回退为类型名加地址格式", "[common][format]")
{
	const WithoutStringifySupport value{7};
	const auto text = spg::xformat("{}", value);

	REQUIRE_FALSE(text.empty());
	REQUIRE(text.find('@') != std::string::npos);
}
