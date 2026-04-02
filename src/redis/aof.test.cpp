#include <sponge/redis/aof.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sponge/tag.h>

namespace fs = std::filesystem;

namespace {

auto read_text_file(const fs::path& path) -> std::string
{
    std::ifstream ifs{ path, std::ios::binary };
    return std::string{ std::istreambuf_iterator<char>{ ifs }, std::istreambuf_iterator<char>{} };
}

void wait_until(std::chrono::milliseconds timeout, const std::function<bool()>& pred)
{
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (pred())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
    }
}

} // namespace

TEST_CASE("AOF追加后持久化命令", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "append.aof";
    fs::remove(file);

    {
        spg::redis::AOF aof{ file.string() };
        aof.append("*1\r\n$4\r\nPING\r\n");
    }

    auto data = read_text_file(file);
    REQUIRE(data.find("PING") != std::string::npos);

    fs::remove(file);
}

TEST_CASE("AOF重置后清空文件", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "reset.aof";
    fs::remove(file);

    {
        spg::redis::AOF aof{ file.string() };
        aof.append("*1\r\n$4\r\nPING\r\n");
        aof.reset();

        wait_until(std::chrono::milliseconds{ 1000 }, [&]() {
            return fs::exists(file) && fs::file_size(file) == 0;
        });
    }

    REQUIRE(fs::exists(file));
    REQUIRE(fs::file_size(file) == 0);

    fs::remove(file);
}

TEST_CASE("AOF后台重写转储字符串键", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "rewrite.aof";
    fs::remove(file);

    std::vector<std::unique_ptr<spg::redis::DBShard>> shards;
    shards.emplace_back(std::make_unique<spg::redis::DBShard>());

    shards[0]->modify("k", [](auto& handler) {
        handler.emplace(spg::as_type<spg::redis::DBShard::String>, "v");
    });

    {
        spg::redis::AOF aof{ file.string() };
        aof.background_rewrite(shards);

        wait_until(std::chrono::milliseconds{ 1500 }, [&]() {
            return fs::exists(file) && fs::file_size(file) > 0;
        });
    }

    auto data = read_text_file(file);
    REQUIRE(data.find("SET") != std::string::npos);
    REQUIRE(data.find("k") != std::string::npos);
    REQUIRE(data.find("v") != std::string::npos);

    fs::remove(file);
}

TEST_CASE("AOF重写时应使用EX编码整数", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "rewrite-integral.aof";
    fs::remove(file);

    std::vector<std::unique_ptr<spg::redis::DBShard>> shards;
    shards.emplace_back(std::make_unique<spg::redis::DBShard>());

    shards[0]->modify("n", [](auto& handler) {
        handler.emplace(spg::as_type<spg::redis::DBShard::Integral>, 42);
        handler.expire(5000);
    });

    {
        spg::redis::AOF aof{ file.string() };
        aof.background_rewrite(shards);

        wait_until(std::chrono::milliseconds{ 1500 }, [&]() {
            return fs::exists(file) && fs::file_size(file) > 0;
        });
    }

    auto data = read_text_file(file);
    REQUIRE(data.find("SET") != std::string::npos);
    REQUIRE(data.find("n") != std::string::npos);
    REQUIRE(data.find("42") != std::string::npos);
    REQUIRE(data.find("EX") != std::string::npos);

    fs::remove(file);
}

TEST_CASE("AOF重写时应编码哈希值", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "rewrite-hash-list.aof";
    fs::remove(file);

    std::vector<std::unique_ptr<spg::redis::DBShard>> shards;
    shards.emplace_back(std::make_unique<spg::redis::DBShard>());

    shards[0]->modify("h", [](auto& handler) {
        auto* table = handler.emplace(spg::as_type<spg::redis::DBShard::HashTable>);
        table->insert_or_assign(std::pmr::string{ "f1" }, std::pmr::string{ "v1" });
        table->insert_or_assign(std::pmr::string{ "f2" }, std::pmr::string{ "v2" });
    });

    {
        spg::redis::AOF aof{ file.string() };
        aof.background_rewrite(shards);

        wait_until(std::chrono::milliseconds{ 1500 }, [&]() {
            return fs::exists(file) && fs::file_size(file) > 0;
        });
    }

    auto data = read_text_file(file);
    REQUIRE(data.find("HSET") != std::string::npos);
    REQUIRE(data.find("h") != std::string::npos);
    REQUIRE(data.find("f1") != std::string::npos);
    REQUIRE(data.find("v1") != std::string::npos);

    fs::remove(file);
}

TEST_CASE("AOF重写时应编码列表值", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "rewrite-hash-list.aof";
    fs::remove(file);

    std::vector<std::unique_ptr<spg::redis::DBShard>> shards;
    shards.emplace_back(std::make_unique<spg::redis::DBShard>());

    shards[0]->modify("l", [](auto& handler) {
        auto* list = handler.emplace(spg::as_type<spg::redis::DBShard::List>);
        list->push_back("e1");
        list->push_back("e2");
    });

    {
        spg::redis::AOF aof{ file.string() };
        aof.background_rewrite(shards);

        wait_until(std::chrono::milliseconds{ 1500 }, [&]() {
            return fs::exists(file) && fs::file_size(file) > 0;
        });
    }

    auto data = read_text_file(file);
    REQUIRE(data.find("RPUSH") != std::string::npos);
    REQUIRE(data.find("l") != std::string::npos);
    REQUIRE(data.find("e1") != std::string::npos);

    fs::remove(file);
}

TEST_CASE("AOF重写时应编码有序集合值", "[redis][aof]")
{
    auto dir = fs::temp_directory_path() / "sponge-aof-test";
    fs::create_directories(dir);

    auto file = dir / "rewrite-zset.aof";
    fs::remove(file);

    std::vector<std::unique_ptr<spg::redis::DBShard>> shards;
    shards.emplace_back(std::make_unique<spg::redis::DBShard>());

    shards[0]->modify("zs", [](auto& handler) {
        auto* zset = handler.emplace(spg::as_type<spg::redis::DBShard::ZSet>);
        zset->add(1.5, "m1");
        zset->add(2.5, "m2");
    });

    {
        spg::redis::AOF aof{ file.string() };
        aof.background_rewrite(shards);

        wait_until(std::chrono::milliseconds{ 1500 }, [&]() {
            return fs::exists(file) && fs::file_size(file) > 0;
        });
    }

    auto data = read_text_file(file);
    REQUIRE(data.find("ZADD") != std::string::npos);
    REQUIRE(data.find("zs") != std::string::npos);
    REQUIRE(data.find("m1") != std::string::npos);
    REQUIRE(data.find("1.5") != std::string::npos);
    REQUIRE(data.find("m2") != std::string::npos);
    REQUIRE(data.find("2.5") != std::string::npos);

    fs::remove(file);
}
