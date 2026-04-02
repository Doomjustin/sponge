#include <sponge/named_type.h>

#include <sstream>
#include <string>
#include <unordered_set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

using UserId = spg::NamedType<int, "user_id", spg::Comparable, spg::Hashable, spg::Printable>;
using Counter = spg::NamedType<int, "counter", spg::Comparable, spg::Incrementable, spg::Decrementable>;
using Distance = spg::NamedType<int, "distance", spg::Comparable, spg::Arithmetic>;
using Ratio = spg::NamedType<double, "ratio", spg::Comparable, spg::Arithmetic>;
using PermissionBits = spg::NamedType<unsigned, "permission_bits", spg::Comparable, spg::Bitwise>;

template <typename T>
concept can_make_named_type = requires 
{
	typename spg::NamedType<T, "probe">;
};

} // namespace

TEST_CASE("NamedType::get() 应返回构造时保存的底层值", "[common][named_type]")
{
	const UserId id{42};

	REQUIRE(id.get() == 42);
}

TEST_CASE("Comparable skill 应支持同名类型之间的相等比较", "[common][named_type]")
{
	const UserId lhs{7};
	const UserId rhs{7};

	REQUIRE(lhs == rhs);
}

TEST_CASE("Comparable skill 应支持同名类型之间的三路比较", "[common][named_type]")
{
	const UserId lhs{3};
	const UserId rhs{9};

	REQUIRE((lhs <=> rhs) < 0);
}

TEST_CASE("Incrementable 与 Decrementable skill 应修改底层值", "[common][named_type]")
{
	Counter counter{3};

	++counter;
	REQUIRE(counter.get() == 4);

	const auto previous = counter--;
	REQUIRE(previous.get() == 4);
	REQUIRE(counter.get() == 3);
}

TEST_CASE("Arithmetic skill 应支持同名类型相加", "[common][named_type]")
{
	const Distance lhs{10};
	const Distance rhs{5};

	const auto result = lhs + rhs;

	REQUIRE(result.get() == 15);
}

TEST_CASE("Arithmetic skill 应支持同名类型相减", "[common][named_type]")
{
	const Distance lhs{10};
	const Distance rhs{5};

	const auto result = lhs - rhs;

	REQUIRE(result.get() == 5);
}

TEST_CASE("Arithmetic skill 应支持同名类型相乘", "[common][named_type]")
{
	const Distance lhs{6};
	const Distance rhs{7};

	const auto result = lhs * rhs;

	REQUIRE(result.get() == 42);
}

TEST_CASE("Arithmetic skill 应支持同名类型相除", "[common][named_type]")
{
	const Distance lhs{21};
	const Distance rhs{3};

	const auto result = lhs / rhs;

	REQUIRE(result.get() == 7);
}

TEST_CASE("Arithmetic skill 应支持同名类型的加法赋值", "[common][named_type]")
{
	Distance lhs{10};
	const Distance rhs{5};

	lhs += rhs;

	REQUIRE(lhs.get() == 15);
}

TEST_CASE("Arithmetic skill 应包含自增与自减能力", "[common][named_type]")
{
	Distance value{8};

	const auto before_inc = value++;
	REQUIRE(before_inc.get() == 8);
	REQUIRE(value.get() == 9);

	const auto after_dec = --value;
	REQUIRE(after_dec.get() == 8);
	REQUIRE(value.get() == 8);
}

TEST_CASE("Arithmetic skill 应支持同名类型的减法赋值", "[common][named_type]")
{
	Distance lhs{10};
	const Distance rhs{5};

	lhs -= rhs;

	REQUIRE(lhs.get() == 5);
}

TEST_CASE("Arithmetic skill 应支持同名类型的乘法赋值", "[common][named_type]")
{
	Distance lhs{6};
	const Distance rhs{7};

	lhs *= rhs;

	REQUIRE(lhs.get() == 42);
}

TEST_CASE("Arithmetic skill 应支持同名类型的除法赋值", "[common][named_type]")
{
	Distance lhs{21};
	const Distance rhs{3};

	lhs /= rhs;

	REQUIRE(lhs.get() == 7);
}

TEST_CASE("Arithmetic skill 应支持同名类型取模", "[common][named_type]")
{
	const Distance lhs{10};
	const Distance rhs{4};

	const auto result = lhs % rhs;

	REQUIRE(result.get() == 2);
}

TEST_CASE("Arithmetic skill 应支持同名类型的取模赋值", "[common][named_type]")
{
	Distance lhs{10};
	const Distance rhs{4};

	lhs %= rhs;

	REQUIRE(lhs.get() == 2);
}

TEST_CASE("Arithmetic skill 在浮点底层类型上应使用 fmod 计算余数", "[common][named_type]")
{
	const Ratio lhs{7.5};
	const Ratio rhs{2.0};

	const auto result = lhs % rhs;

	REQUIRE(result.get() == Catch::Approx(1.5));
}

TEST_CASE("Hashable skill 应使 NamedType 可用于 unordered_set", "[common][named_type]")
{
	std::unordered_set<UserId> ids;

	ids.insert(UserId{1});
	ids.insert(UserId{2});

	REQUIRE(ids.contains(UserId{1}));
	REQUIRE(ids.contains(UserId{2}));
	REQUIRE_FALSE(ids.contains(UserId{3}));
}

TEST_CASE("Printable skill 应输出底层值的文本表示", "[common][named_type]")
{
	std::ostringstream os;
	const UserId id{128};

	os << id;

	REQUIRE(os.str() == "128");
}

TEST_CASE("Bitwise skill 应支持同名整型之间的按位与", "[common][named_type]")
{
	const PermissionBits lhs{0b1101U};
	const PermissionBits rhs{0b1011U};

	const auto result = lhs & rhs;

	REQUIRE(result.get() == 0b1001U);
}

TEST_CASE("Bitwise skill 应支持同名整型之间的按位或", "[common][named_type]")
{
	const PermissionBits lhs{0b1101U};
	const PermissionBits rhs{0b1011U};

	const auto result = lhs | rhs;

	REQUIRE(result.get() == 0b1111U);
}

TEST_CASE("Bitwise skill 应支持同名整型之间的按位异或", "[common][named_type]")
{
	const PermissionBits lhs{0b1101U};
	const PermissionBits rhs{0b1011U};

	const auto result = lhs ^ rhs;

	REQUIRE(result.get() == 0b0110U);
}

TEST_CASE("Bitwise skill 应支持同名整型的按位与赋值", "[common][named_type]")
{
	PermissionBits lhs{0b1101U};
	const PermissionBits rhs{0b1011U};

	lhs &= rhs;

	REQUIRE(lhs.get() == 0b1001U);
}

TEST_CASE("Bitwise skill 应支持同名整型的按位或赋值", "[common][named_type]")
{
	PermissionBits lhs{0b1101U};
	const PermissionBits rhs{0b1011U};

	lhs |= rhs;

	REQUIRE(lhs.get() == 0b1111U);
}

TEST_CASE("Bitwise skill 应支持同名整型的按位异或赋值", "[common][named_type]")
{
	PermissionBits lhs{0b1101U};
	const PermissionBits rhs{0b1011U};

	lhs ^= rhs;

	REQUIRE(lhs.get() == 0b0110U);
}

TEST_CASE("NamedType 应允许使用算术类型作为底层类型", "[common][named_type]")
{
	REQUIRE(can_make_named_type<int>);
	REQUIRE(can_make_named_type<double>);
}

TEST_CASE("NamedType 不应允许使用非算术类型作为底层类型", "[common][named_type]")
{
	REQUIRE_FALSE(can_make_named_type<std::string>);
}
