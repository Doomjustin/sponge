#include "log.h"

#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sponge/coding.h>
#include <sponge/leveldb/exceptions.h>

#include "crc.h"

namespace {

namespace asio = boost::asio;
namespace fs = std::filesystem;
namespace wal = spg::leveldb::wal;

struct ParsedRecord {
	uint32_t checksum;
	uint16_t length;
	spg::leveldb::RecordType type;
	std::string_view payload;
};

class TempLogFile {
public:
	TempLogFile()
	  : path_{ fs::temp_directory_path() / fs::path{ "sponge-leveldb-log-test.log" } }
	{
		std::error_code ec;
		fs::remove(path_, ec);
	}

	~TempLogFile()
	{
		std::error_code ec;
		fs::remove(path_, ec);
	}

	[[nodiscard]] auto path() const -> const fs::path& { return path_; }

private:
	fs::path path_;
};

auto read_all_bytes(const fs::path& path) -> std::string
{
	std::ifstream is(path, std::ios::binary);
	REQUIRE(is.is_open());

	return { std::istreambuf_iterator<char>{ is }, std::istreambuf_iterator<char>{} };
}

auto run_writer_once(const fs::path& path, const std::string_view record) -> void
{
	asio::io_context io;
	std::exception_ptr ep;

	asio::co_spawn(
		io,
		[&]() -> asio::awaitable<void> {
			wal::Writer writer{ co_await asio::this_coro::executor, path };
			co_await writer.async_append(record);
			writer.sync();
		},
		[&](std::exception_ptr e) { ep = e; }
	);

	io.run();
	REQUIRE(ep == nullptr);
}

auto run_writer_many(const fs::path& path, const std::vector<std::string>& records) -> void
{
	asio::io_context io;
	std::exception_ptr ep;

	asio::co_spawn(
		io,
		[&]() -> asio::awaitable<void> {
			wal::Writer writer{ co_await asio::this_coro::executor, path };
			for (const auto& record : records)
				co_await writer.async_append(record);
			writer.sync();
		},
		[&](std::exception_ptr e) { ep = e; }
	);

	io.run();
	REQUIRE(ep == nullptr);
}

auto read_all_records(const fs::path& path, const bool paranoid_checks = false) -> std::vector<std::string>
{
	asio::io_context io;
	asio::stream_file file{ io.get_executor(), path, asio::stream_file::read_only };
	wal::Reader reader{ file, path, paranoid_checks };

	std::vector<std::string> records;
	for (auto it = reader.begin(); it != reader.end(); ++it)
		records.emplace_back(*it);

	return records;
}

auto parse_first_record(const std::string& bytes) -> ParsedRecord
{
	const auto [checksum, p_after_checksum] = spg::fixed::decode<uint32_t>(bytes.data());
	const auto [length, p_after_length] = spg::fixed::decode<uint16_t>(p_after_checksum);
	const auto type = static_cast<spg::leveldb::RecordType>(static_cast<uint8_t>(*p_after_length));
	const std::string_view payload{ p_after_length + 1, length };

	return { checksum, length, type, payload };
}

auto parse_all_records(const std::string& bytes) -> std::vector<ParsedRecord>
{
	std::vector<ParsedRecord> records;

	const char* cur = bytes.data();
	const char* const end = bytes.data() + bytes.size();

	while (cur < end) {
		REQUIRE(static_cast<size_t>(end - cur) >= 7);

		const auto [checksum, p_after_checksum] = spg::fixed::decode<uint32_t>(cur);
		const auto [length, p_after_length] = spg::fixed::decode<uint16_t>(p_after_checksum);
		const auto type = static_cast<spg::leveldb::RecordType>(static_cast<uint8_t>(*p_after_length));
		const char* payload_begin = p_after_length + 1;

		REQUIRE(static_cast<size_t>(end - payload_begin) >= length);

		records.push_back({
			.checksum = checksum,
			.length = length,
			.type = type,
			.payload = std::string_view{ payload_begin, length },
		});

		cur = payload_begin + length;
	}

	return records;
}

auto make_large_payload(const size_t length) -> std::string
{
	std::string payload(length, '\0');
	for (size_t i = 0; i < length; ++i)
		payload[i] = static_cast<char>('a' + (i % 26));

	return payload;
}

auto overwrite_file_bytes(const fs::path& path, const std::string& bytes) -> void
{
	std::ofstream os(path, std::ios::binary | std::ios::trunc);
	REQUIRE(os.is_open());
	os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	os.flush();
}

} // namespace

TEST_CASE("在写入单条记录时，应该生成 header 加 payload 的完整字节长度", "[leveldb][log][writer]")
{
	TempLogFile temp;
	constexpr auto payload = "wal-entry";

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	REQUIRE(bytes.size() == 7 + std::char_traits<char>::length(payload));

	const auto [checksum, p_after_checksum] = spg::fixed::decode<uint32_t>(bytes.data());
	const auto [length, p_after_length] = spg::fixed::decode<uint16_t>(p_after_checksum);
	const auto type = static_cast<spg::leveldb::RecordType>(static_cast<uint8_t>(*p_after_length));
	const std::string_view stored_payload{ p_after_length + 1, length };

	REQUIRE(length == std::char_traits<char>::length(payload));
	REQUIRE(type == spg::leveldb::RecordType::Full);
	REQUIRE(stored_payload == payload);
	REQUIRE(spg::leveldb::crc::verify(spg::leveldb::crc::masked(checksum), std::to_underlying(type), stored_payload));
}

TEST_CASE("Writer 写入空字符串时应产生空 payload 的 Full record", "[leveldb][log][writer]")
{
	TempLogFile temp;

	run_writer_once(temp.path(), "");

	const auto bytes = read_all_bytes(temp.path());
	REQUIRE(bytes.size() == 7);
}

TEST_CASE("在写入单条记录时，应该将 record type 编码为 Full", "[leveldb][log][writer]")
{
	TempLogFile temp;
	constexpr auto payload = "wal-entry";

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto record = parse_first_record(bytes);

	REQUIRE(record.type == spg::leveldb::RecordType::Full);
}

TEST_CASE("在写入单条记录时，应该原样写入 payload 内容", "[leveldb][log][writer]")
{
	TempLogFile temp;
	constexpr auto payload = "wal-entry";

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto record = parse_first_record(bytes);

	REQUIRE(record.payload == payload);
}

TEST_CASE("在写入单条记录时，应该写入可校验通过的 checksum", "[leveldb][log][writer]")
{
	TempLogFile temp;
	constexpr auto payload = "wal-entry";

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto record = parse_first_record(bytes);

	REQUIRE(spg::leveldb::crc::verify(spg::leveldb::crc::masked(record.checksum), std::to_underlying(record.type), record.payload));
}

TEST_CASE("在写入空字符串时，应该只生成 header 字节", "[leveldb][log][writer]")
{
	TempLogFile temp;

	run_writer_once(temp.path(), "");

	const auto bytes = read_all_bytes(temp.path());
	REQUIRE(bytes.size() == 7);
}

TEST_CASE("在写入空字符串时，应该将 payload 长度编码为 0", "[leveldb][log][writer]")
{
	TempLogFile temp;

	run_writer_once(temp.path(), "");

	const auto bytes = read_all_bytes(temp.path());
	const auto record = parse_first_record(bytes);

	REQUIRE(record.length == 0);
}

TEST_CASE("在写入空字符串时，应该将 record type 编码为 Full", "[leveldb][log][writer]")
{
	TempLogFile temp;

	run_writer_once(temp.path(), "");

	const auto bytes = read_all_bytes(temp.path());
	const auto record = parse_first_record(bytes);

	REQUIRE(record.type == spg::leveldb::RecordType::Full);
}

TEST_CASE("在写入空字符串时，应该写入可校验通过的 checksum", "[leveldb][log][writer]")
{
	TempLogFile temp;

	run_writer_once(temp.path(), "");

	const auto bytes = read_all_bytes(temp.path());
	const auto record = parse_first_record(bytes);

	REQUIRE(spg::leveldb::crc::verify(spg::leveldb::crc::masked(record.checksum), std::to_underlying(record.type), std::string_view{}));
}

TEST_CASE("在写入超大记录时，应该拆分成多个 physical record", "[leveldb][log][writer]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto records = parse_all_records(bytes);

	REQUIRE(records.size() > 1);
}

TEST_CASE("在写入超大记录时，应该将首分片类型编码为 First", "[leveldb][log][writer]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto records = parse_all_records(bytes);

	REQUIRE(records.front().type == spg::leveldb::RecordType::First);
}

TEST_CASE("在写入超大记录时，应该将尾分片类型编码为 Last", "[leveldb][log][writer]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto records = parse_all_records(bytes);

	REQUIRE(records.back().type == spg::leveldb::RecordType::Last);
}

TEST_CASE("在写入超大记录时，应该将中间分片类型编码为 Middle", "[leveldb][log][writer]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto records = parse_all_records(bytes);

	REQUIRE(records.size() >= 3);
	for (size_t i = 1; i + 1 < records.size(); ++i)
		REQUIRE(records[i].type == spg::leveldb::RecordType::Middle);
}

TEST_CASE("在写入超大记录时，应该可由分片 payload 重组出原始内容", "[leveldb][log][writer]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto records = parse_all_records(bytes);

	std::string rebuilt;
	rebuilt.reserve(payload.size());
	for (const auto& record : records)
		rebuilt.append(record.payload);

	REQUIRE(rebuilt == payload);
}

TEST_CASE("在写入超大记录时，应该保证每个分片 checksum 都可校验通过", "[leveldb][log][writer]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);

	const auto bytes = read_all_bytes(temp.path());
	const auto records = parse_all_records(bytes);

	for (const auto& record : records)
		REQUIRE(spg::leveldb::crc::verify(spg::leveldb::crc::masked(record.checksum), std::to_underlying(record.type), record.payload));
}

TEST_CASE("在读取单条日志记录时，应该返回对应的记录内容", "[leveldb][log][reader]")
{
	TempLogFile temp;
	constexpr auto payload = "reader-one-record";

	run_writer_once(temp.path(), payload);
	const auto records = read_all_records(temp.path());

	REQUIRE(records.size() == 1);
	REQUIRE(records.front() == payload);
}

TEST_CASE("在读取多条日志记录时，应该保持写入顺序", "[leveldb][log][reader]")
{
	TempLogFile temp;
	const std::vector<std::string> expected{ "record-1", "record-2", "record-3" };

	run_writer_many(temp.path(), expected);
	const auto records = read_all_records(temp.path());

	REQUIRE(records == expected);
}

TEST_CASE("在读取分片日志记录时，应该重组出完整内容", "[leveldb][log][reader]")
{
	TempLogFile temp;
	const auto payload = make_large_payload(200000);

	run_writer_once(temp.path(), payload);
	const auto records = read_all_records(temp.path());

	REQUIRE(records.size() == 1);
	REQUIRE(records.front() == payload);
}

TEST_CASE("在读取 checksum 损坏日志且 paranoid_checks 关闭时，应该返回空结果", "[leveldb][log][reader]")
{
	TempLogFile temp;
	constexpr auto payload = "corrupted-record";

	run_writer_once(temp.path(), payload);
	auto bytes = read_all_bytes(temp.path());
	REQUIRE_FALSE(bytes.empty());
	bytes[0] = static_cast<char>(bytes[0] ^ 0xFF);
	overwrite_file_bytes(temp.path(), bytes);

	const auto records = read_all_records(temp.path(), false);
	REQUIRE(records.empty());
}

TEST_CASE("在读取 checksum 损坏日志且 paranoid_checks 开启时，应该抛出 CorruptionException", "[leveldb][log][reader]")
{
	TempLogFile temp;
	constexpr auto payload = "corrupted-record";

	run_writer_once(temp.path(), payload);
	auto bytes = read_all_bytes(temp.path());
	REQUIRE_FALSE(bytes.empty());
	bytes[0] = static_cast<char>(bytes[0] ^ 0xFF);
	overwrite_file_bytes(temp.path(), bytes);

	REQUIRE_THROWS_AS(read_all_records(temp.path(), true), spg::leveldb::CorruptionException);
}
