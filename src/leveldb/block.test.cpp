#include "block.h"

#include <cstdint>
#include <string_view>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

#include <sponge/coding.h>

namespace {

using spg::leveldb::BlockBuilder;

struct ParsedEntry {
	uint32_t shared;
	uint32_t non_shared;
	uint32_t value_len;
	std::string_view key_suffix;
	std::string_view value;
	const char* next;
};

auto parse_entry(const char* p) -> ParsedEntry
{
	auto [shared, p1]     = spg::varint::decode<uint32_t>(p);
	auto [non_shared, p2] = spg::varint::decode<uint32_t>(p1);
	auto [value_len, p3]  = spg::varint::decode<uint32_t>(p2);
	std::string_view key_suffix{ p3, non_shared };
	std::string_view value{ p3 + non_shared, value_len };
	return { .shared=shared, 
             .non_shared=non_shared, 
             .value_len=value_len, 
             .key_suffix=key_suffix, 
             .value=value,
			 .next=p3 + non_shared + value_len 
            };
}

auto num_restarts(std::string_view data) -> uint32_t
{
	auto [n, _] = spg::fixed::decode<uint32_t>(data.data() + data.size() - sizeof(uint32_t));
	return n;
}

auto restart_offset(std::string_view data, uint32_t index) -> uint32_t
{
	const auto n = num_restarts(data);
	const char* p = data.data() + data.size()
					- sizeof(uint32_t)             // num_restarts field
					- sizeof(uint32_t) * n         // all restart entries
					+ sizeof(uint32_t) * index;
	auto [v, _] = spg::fixed::decode<uint32_t>(p);
	return v;
}

} // namespace

// ---- empty() ---------------------------------------------------------------

TEST_CASE("在初始化 BlockBuilder 时，应该处于空状态", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	REQUIRE(builder.empty());
}

TEST_CASE("在写入一条记录后，应该不再处于空状态", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	REQUIRE_FALSE(builder.empty());
}

TEST_CASE("在 reset 后，应该恢复空状态", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	builder.reset();
	REQUIRE(builder.empty());
}

// ---- estimate_size() -------------------------------------------------------

TEST_CASE("在 build 后，estimate_size 应该与实际输出长度相等", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("aaa", "111");
	builder.add("bbb", "222");
	const auto expected = builder.estimate_size();
	const auto data = builder.build();
	REQUIRE(data.size() == expected);
}

// ---- build() 二进制格式 -----------------------------------------------------

TEST_CASE("在写入单条记录时，应该将 shared_len 编码为 0", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	const auto entry = parse_entry(data.data());
	REQUIRE(entry.shared == 0);
}

TEST_CASE("在写入单条记录时，应该将 non_shared_len 编码为键的长度", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	const auto entry = parse_entry(data.data());
	REQUIRE(entry.non_shared == 3);
}

TEST_CASE("在写入单条记录时，应该将 value_len 编码为值的长度", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	const auto entry = parse_entry(data.data());
	REQUIRE(entry.value_len == 5);
}

TEST_CASE("在写入单条记录时，应该原样写入键内容", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	const auto entry = parse_entry(data.data());
	REQUIRE(entry.key_suffix == "key");
}

TEST_CASE("在写入单条记录时，应该原样写入值内容", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	const auto entry = parse_entry(data.data());
	REQUIRE(entry.value == "value");
}

TEST_CASE("在写入单条记录时，尾部 restart point 数量应为 1", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	REQUIRE(num_restarts(data) == 1);
}

TEST_CASE("在写入单条记录时，首个 restart point 的 offset 应为 0", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	const auto data = builder.build();
	REQUIRE(restart_offset(data, 0) == 0);
}

// ---- 前缀压缩 ---------------------------------------------------------------

TEST_CASE("在写入有共同前缀的键时，应该将 shared_len 编码为共享前缀长度", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("abcde", "1");
	builder.add("abcxy", "2");
	const auto data = builder.build();
	const auto first = parse_entry(data.data());
	const auto second = parse_entry(first.next);
	REQUIRE(second.shared == 3); // "abc"
}

TEST_CASE("在写入有共同前缀的键时，应该只写入非共享的键后缀", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("abcde", "1");
	builder.add("abcxy", "2");
	const auto data = builder.build();
	const auto first = parse_entry(data.data());
	const auto second = parse_entry(first.next);
	REQUIRE(second.key_suffix == "xy");
}

// ---- restart interval -------------------------------------------------------

TEST_CASE("在写入超过 restart_interval 条记录后，应添加第二个 restart point", "[leveldb][block][builder]")
{
	constexpr size_t interval = 4;
	BlockBuilder builder{ interval };
	builder.add("a", "1");
	builder.add("b", "2");
	builder.add("c", "3");
	builder.add("d", "4");
	builder.add("e", "5"); // 触发新 restart point
	const auto data = builder.build();
	REQUIRE(num_restarts(data) == 2);
}

TEST_CASE("在触发新 restart point 时，对应条目的 shared_len 应为 0", "[leveldb][block][builder]")
{
	constexpr size_t interval = 2;
	BlockBuilder builder{ interval };
	builder.add("aaa", "1");
	builder.add("aab", "2");
	builder.add("aac", "3"); // 触发新 restart point，shared_len 应重置为 0
	const auto data = builder.build();
	const auto e1 = parse_entry(data.data());
	const auto e2 = parse_entry(e1.next);
	const auto e3 = parse_entry(e2.next);
	REQUIRE(e3.shared == 0);
}

// ---- reset() ----------------------------------------------------------------

TEST_CASE("在 reset 后，应该可以重新写入并正确 build", "[leveldb][block][builder]")
{
	BlockBuilder builder;
	builder.add("key", "value");
	std::ignore = builder.build();
	builder.reset();
	builder.add("new", "data");
	const auto data = builder.build();
	const auto entry = parse_entry(data.data());
	REQUIRE(entry.key_suffix == "new");
	REQUIRE(entry.value == "data");
}
