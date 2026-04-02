#include <sponge/format.h>
#include <sponge/log.h>

#include <catch2/catch_test_macros.hpp>

namespace {

class MockLogger : public spg::Logger {
public:
	struct Entry {
		spg::LogLevel level;
		std::string message;
	};

	std::vector<Entry> entries;
	spg::LogLevel impl_level{spg::LogLevel::Info};
	std::string impl_pattern;

private:
	void log(spg::LogLevel level, std::string_view message) override
	{
		entries.push_back({level, std::string{message}});
	}

	void set_level_impl(spg::LogLevel level) noexcept override
	{
		impl_level = level;
	}

	void set_pattern_impl(std::string_view pattern) override
	{
		impl_pattern = std::string{pattern};
	}
};

} // namespace

TEST_CASE("Logger::set_level() 设置后 level() 应返回相同值", "[common][log]")
{
	MockLogger logger;
	logger.set_level(spg::LogLevel::Debug);

	REQUIRE(logger.level() == spg::LogLevel::Debug);
}

TEST_CASE("Logger::set_level() 应调用底层 set_level_impl", "[common][log]")
{
	MockLogger logger;
	logger.set_level(spg::LogLevel::Error);

	REQUIRE(logger.impl_level == spg::LogLevel::Error);
}

TEST_CASE("Logger::set_pattern() 应调用底层 set_pattern_impl", "[common][log]")
{
	MockLogger logger;
	logger.set_pattern("[%H:%M:%S] %v");

	REQUIRE(logger.impl_pattern == "[%H:%M:%S] %v");
}

TEST_CASE("Logger::trace() 应以 Trace 级别记录消息", "[common][log]")
{
	MockLogger logger;
	logger.trace("hello trace");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].level == spg::LogLevel::Trace);
	REQUIRE(logger.entries[0].message == "hello trace");
}

TEST_CASE("Logger::debug() 应以 Debug 级别记录消息", "[common][log]")
{
	MockLogger logger;
	logger.debug("hello debug");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].level == spg::LogLevel::Debug);
	REQUIRE(logger.entries[0].message == "hello debug");
}

TEST_CASE("Logger::info() 应以 Info 级别记录消息", "[common][log]")
{
	MockLogger logger;
	logger.info("hello info");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].level == spg::LogLevel::Info);
	REQUIRE(logger.entries[0].message == "hello info");
}

TEST_CASE("Logger::warning() 应以 Warning 级别记录消息", "[common][log]")
{
	MockLogger logger;
	logger.warning("hello warning");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].level == spg::LogLevel::Warning);
	REQUIRE(logger.entries[0].message == "hello warning");
}

TEST_CASE("Logger::error() 应以 Error 级别记录消息", "[common][log]")
{
	MockLogger logger;
	logger.error("hello error");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].level == spg::LogLevel::Error);
	REQUIRE(logger.entries[0].message == "hello error");
}

TEST_CASE("Logger::critical() 应以 Critical 级别记录消息", "[common][log]")
{
	MockLogger logger;
	logger.critical("hello critical");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].level == spg::LogLevel::Critical);
	REQUIRE(logger.entries[0].message == "hello critical");
}

TEST_CASE("Logger::info() 应格式化参数后传入 log", "[common][log]")
{
	MockLogger logger;
	logger.info("value={} name={}", 42, "spg");

	REQUIRE(logger.entries.size() == 1);
	REQUIRE(logger.entries[0].message == "value=42 name=spg");
}

TEST_CASE("log::set_default_logger() 替换后应路由到新 Logger", "[common][log]")
{
	auto mock = std::make_unique<MockLogger>();
	auto& mock_ref = *mock;
	spg::log::set_default_logger(std::move(mock));

	spg::log::info("facade test");

	REQUIRE(mock_ref.entries.size() == 1);
	REQUIRE(mock_ref.entries[0].message == "facade test");
}

TEST_CASE("log::set_level() 应传播至底层 Logger 并可经 log::level() 读回", "[common][log]")
{
	spg::log::set_default_logger(std::make_unique<MockLogger>());

	spg::log::set_level(spg::LogLevel::Debug);

	REQUIRE(spg::log::level() == spg::LogLevel::Debug);
}
