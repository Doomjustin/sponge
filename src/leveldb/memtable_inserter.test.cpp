#include "memtable_inserter.h"

#include <string_view>
#include <variant>

#include <catch2/catch_test_macros.hpp>

using namespace std::literals;

using namespace spg::leveldb;
using namespace spg::leveldb::literals;

TEST_CASE("apply_to 应将 WriteBatch 的 put/erase 应用到 MemTable", "[leveldb][memtable_inserter]")
{
	WriteBatch batch;
	batch.set_sequence(10_seq);
	batch.put("alpha", "v1");
	batch.erase("beta");
	batch.put("alpha", "v2");

	MemTable table;
	apply_to(batch, table);

	const auto alpha_latest = table.get("alpha", 12_seq);
	REQUIRE(std::holds_alternative<std::string_view>(alpha_latest));
	CHECK(std::get<std::string_view>(alpha_latest) == "v2"sv);

	const auto alpha_old = table.get("alpha", 10_seq);
	REQUIRE(std::holds_alternative<std::string_view>(alpha_old));
	CHECK(std::get<std::string_view>(alpha_old) == "v1"sv);

	const auto beta_latest = table.get("beta", 12_seq);
	REQUIRE(std::holds_alternative<Deleted>(beta_latest));

	const auto beta_before_delete = table.get("beta", 10_seq);
	REQUIRE(std::holds_alternative<NotFound>(beta_before_delete));
}

TEST_CASE("apply_to 在空 WriteBatch 上不应改变 MemTable", "[leveldb][memtable_inserter]")
{
	WriteBatch batch;
	batch.set_sequence(99_seq);

	MemTable table;
	table.add(3_seq, ValueType::Value, "x", "old");

	apply_to(batch, table);

	const auto result = table.get("x", 100_seq);
	REQUIRE(std::holds_alternative<std::string_view>(result));
	CHECK(std::get<std::string_view>(result) == "old"sv);
}
