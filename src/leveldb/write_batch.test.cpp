#include <sponge/leveldb/write_batch.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sponge/coding.h>
#include <sponge/leveldb/exceptions.h>

using namespace std::literals;

using namespace spg::leveldb;
using namespace spg::leveldb::literals;

namespace {

struct RecordingHandler {
	std::vector<std::pair<std::string, std::string>> puts;
	std::vector<std::string> erases;

	void put(const std::string_view key, const std::string_view value)
	{
		puts.emplace_back(key, value);
	}

	void erase(const std::string_view key)
	{
		erases.emplace_back(key);
	}
};

} // namespace

TEST_CASE("WriteBatch iterate 应按追加顺序回放 put/erase 记录", "[leveldb][write_batch]")
{
	WriteBatch batch;

	batch.put("k1", "v1");
	batch.erase("k2");
	batch.put("k3", "v3");

	RecordingHandler handler;
	batch.iterate(handler);

	REQUIRE(handler.puts.size() == 2);
	REQUIRE(handler.erases.size() == 1);

	REQUIRE(handler.puts[0] == std::pair{ "k1"s, "v1"s });
	REQUIRE(handler.puts[1] == std::pair{ "k3"s, "v3"s });
	REQUIRE(handler.erases[0] == "k2");
}

TEST_CASE("WriteBatch sequence/clear 应维护头部状态", "[leveldb][write_batch]")
{
	WriteBatch batch;

	batch.set_sequence(42_seq);
	batch.put("alpha", "value");

	REQUIRE(batch.sequence() == 42_seq);
	REQUIRE(batch.approximate_size() > sizeof(SequenceNumber::value_type) + sizeof(uint32_t));

	batch.clear();

	REQUIRE(batch.sequence() == 0_seq);
	REQUIRE(batch.encode().size() == sizeof(SequenceNumber::value_type) + sizeof(uint32_t));

	RecordingHandler handler;
	batch.iterate(handler);
	REQUIRE(handler.puts.empty());
	REQUIRE(handler.erases.empty());
}

TEST_CASE("WriteBatch 在 count 大于实际记录数时 iterate 应抛出 CorruptionException", "[leveldb][write_batch]")
{
	WriteBatch batch;
	batch.put("key", "value");

	auto payload = batch.encode();
	auto* mutable_data = const_cast<char*>(payload.data());
	constexpr auto count_offset = sizeof(SequenceNumber::value_type);
	spg::fixed::encode(mutable_data + count_offset, static_cast<uint32_t>(2));

	RecordingHandler handler;
	REQUIRE_THROWS_AS(batch.iterate(handler), CorruptionException);
}

