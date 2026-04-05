#include "crc.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <crc32c/crc32c.h>

using namespace std::literals;

using spg::leveldb::crc;

namespace {

auto compute_raw_crc(const uint8_t type, const std::string_view payload) -> uint32_t
{
	uint32_t result = 0;
	result = crc32c::Extend(result, &type, 1);
	result = crc32c::Extend(result, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
	return result;
}

} // namespace

TEST_CASE("crc::compute 对同一输入应返回稳定结果", "[leveldb][crc]")
{
	constexpr uint8_t type = 1;
	constexpr auto payload = "hello crc"sv;

	const auto first = crc::compute(type, payload);
	const auto second = crc::compute(type, payload);

	REQUIRE(first == second);
}

TEST_CASE("crc::compute 应区分 record type", "[leveldb][crc]")
{
	constexpr auto payload = "same payload"sv;

	const auto type1_crc = crc::compute(1, payload);
	const auto type2_crc = crc::compute(2, payload);

	REQUIRE(type1_crc != type2_crc);
}

TEST_CASE("crc::compute 应区分 payload 内容", "[leveldb][crc]")
{
	constexpr uint8_t type = 7;

	const auto crc_a = crc::compute(type, "payload-a");
	const auto crc_b = crc::compute(type, "payload-b");

	REQUIRE(crc_a != crc_b);
}

TEST_CASE("crc::verity 在匹配输入时应返回 true", "[leveldb][crc]")
{
	constexpr uint8_t type = 3;
	constexpr auto payload = "verification"sv;

	const auto expected = crc::compute(type, payload);

	REQUIRE(crc::verify(expected, type, payload));
}

TEST_CASE("crc::verity 在 type 或 payload 变化时应返回 false", "[leveldb][crc]")
{
	constexpr uint8_t type = 5;
	constexpr auto payload = "segment-data"sv;

	const auto expected = crc::compute(type, payload);

	REQUIRE_FALSE(crc::verify(expected, static_cast<uint8_t>(type + 1), payload));
	REQUIRE_FALSE(crc::verify(expected, type, "segment-data-corrupted"));
}

TEST_CASE("crc::verity 应支持空 payload", "[leveldb][crc]")
{
	constexpr uint8_t type = 9;
	constexpr auto payload = ""sv;

	const auto expected = crc::compute(type, payload);

	REQUIRE(crc::verify(expected, type, payload));
}

TEST_CASE("crc::compute 的默认重载应返回未掩码的原始 CRC", "[leveldb][crc]")
{
	constexpr uint8_t type = 11;
	constexpr auto payload = "raw-verification"sv;

	const auto raw_crc = compute_raw_crc(type, payload);
	const auto actual = crc::compute(type, payload);

	REQUIRE(actual == raw_crc);
	REQUIRE(crc::verify(actual, type, payload));
}

TEST_CASE("crc::compute 的 add_mask 重载应返回 masked CRC", "[leveldb][crc]")
{
	constexpr uint8_t type = 12;
	constexpr auto payload = "masked-verification"sv;

	const auto raw_crc = crc::compute(type, payload);
	const auto masked_crc = crc::compute(type, payload, crc::add_mask);

	REQUIRE(masked_crc != raw_crc);
	REQUIRE(crc::verify(crc::masked(masked_crc), type, payload));
	REQUIRE_FALSE(crc::verify(crc::masked(raw_crc), type, payload));
}

TEST_CASE("crc::verity 的 uint32_t 重载不应接受 masked CRC", "[leveldb][crc]")
{
	constexpr uint8_t type = 13;
	constexpr auto payload = "raw-overload"sv;

	const auto masked_crc = crc::compute(type, payload, crc::add_mask);

	REQUIRE_FALSE(crc::verify(masked_crc, type, payload));
}
