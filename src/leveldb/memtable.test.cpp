#include <sponge/leveldb/memtable.h>

#include <string_view>
#include <variant>

#include <catch2/catch_test_macros.hpp>

using namespace std::literals;

using namespace spg::leveldb;

TEST_CASE("MemTable get 在命中 value 记录时应返回对应值", "[leveldb][memtable]")
{
	MemTable table;

	table.add(SequenceNumber{ 7 }, ValueType::Value, "alpha", "v1");

	const auto result = table.get("alpha", SequenceNumber{ 7 });
	REQUIRE(std::holds_alternative<std::string_view>(result));
	CHECK(std::get<std::string_view>(result) == "v1"sv);
}

TEST_CASE("MemTable get 在 key 不存在时应返回 NotFound", "[leveldb][memtable]")
{
	MemTable table;

	const auto result = table.get("missing", SequenceNumber{ 1 });
	REQUIRE(std::holds_alternative<NotFound>(result));
}

TEST_CASE("MemTable get 应返回不晚于查询序号的最新版本", "[leveldb][memtable]")
{
	MemTable table;

	table.add(SequenceNumber{ 3 }, ValueType::Value, "alpha", "old");
	table.add(SequenceNumber{ 9 }, ValueType::Value, "alpha", "new");

	const auto latest = table.get("alpha", SequenceNumber{ 9 });
	REQUIRE(std::holds_alternative<std::string_view>(latest));
	CHECK(std::get<std::string_view>(latest) == "new"sv);

	const auto historical = table.get("alpha", SequenceNumber{ 3 });
	REQUIRE(std::holds_alternative<std::string_view>(historical));
	CHECK(std::get<std::string_view>(historical) == "old"sv);
}

TEST_CASE("MemTable get 在最新版本是删除标记时应返回 Deleted", "[leveldb][memtable]")
{
	MemTable table;

	table.add(SequenceNumber{ 3 }, ValueType::Value, "alpha", "old");
	table.add(SequenceNumber{ 9 }, ValueType::Deletion, "alpha", "");

	const auto deleted = table.get("alpha", SequenceNumber{ 9 });
	REQUIRE(std::holds_alternative<Deleted>(deleted));

	const auto historical = table.get("alpha", SequenceNumber{ 3 });
	REQUIRE(std::holds_alternative<std::string_view>(historical));
	CHECK(std::get<std::string_view>(historical) == "old"sv);
}
