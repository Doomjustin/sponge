#include "dash_table.h"

#include <mutex>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using spg::redis::DashTable;
using spg::read_write;
using spg::read_only;
using spg::unlock;

TEST_CASE("dash_table 基础set/get/erase", "[redis][dash_table]")
{
    DashTable<int> table;

    REQUIRE(table.empty());
    REQUIRE(table.size() == 0);

    table.set("k1", 7);
    table.set("k2", 9);

    REQUIRE_FALSE(table.empty());
    REQUIRE(table.size() == 2);

    auto* v1 = table.get("k1");
    REQUIRE(v1 != nullptr);
    CHECK(*v1 == 7);

    CHECK(table.erase("k1"));
    CHECK_FALSE(table.erase("k1"));
    CHECK(table.get("k1") == nullptr);
    CHECK(table.size() == 1);
}

TEST_CASE("dash_table modify应更新已有值", "[redis][dash_table]")
{
    DashTable<int> table;
    table.set("counter", 1);

    table.modify("counter", [](auto& segment, auto it) {
        REQUIRE(it != segment.end());
        ++it->second;
        return 0;
    });

    auto* updated = table.get("counter");
    REQUIRE(updated != nullptr);
    CHECK(*updated == 2);
}

TEST_CASE("dash_table unlock路径应支持无锁访问与修改", "[redis][dash_table]")
{
    DashTable<int> table;
    table.set("counter", 2);

    auto& mutex = table.mutex_of("counter");
    std::unique_lock locker{ mutex };

    auto* raw = table.get("counter", unlock);
    REQUIRE(raw != nullptr);
    CHECK(*raw == 2);

    table.set("counter", 11, unlock);
    CHECK(table.erase("counter", unlock));
    CHECK(table.get("counter", unlock) == nullptr);
}

TEST_CASE("dash_table range遍历空表所有分段", "[redis][dash_table]")
{
    DashTable<int> table;

    size_t segment_count = 0;
    size_t total_entries = 0;

    for (auto view : table.range(read_only)) {
        ++segment_count;
        for (const auto& [k, v] : view) {
            (void)k;
            (void)v;
            ++total_entries;
        }
    }

    CHECK(segment_count == 1024);
    CHECK(total_entries == 0);
}

TEST_CASE("dash_table range遍历已插入条目", "[redis][dash_table]")
{
    DashTable<int> table;
    table.set("a", 1);
    table.set("b", 2);
    table.set("c", 3);

    int sum = 0;
    size_t count = 0;

    for (auto view : table.range(read_only)) {
        for (const auto& [key, value] : view) {
            (void)key;
            sum += value;
            ++count;
        }
    }

    CHECK(count == 3);
    CHECK(sum == 6);
}

TEST_CASE("dash_table modify range更新所有值", "[redis][dash_table]")
{
    DashTable<int> table;
    table.set("x", 1);
    table.set("y", 2);
    table.set("z", 3);

    size_t touched = 0;
    for (auto view : table.range(read_write)) {
        for (auto& [key, value] : view) {
            (void)key;
            value += 10;
            ++touched;
        }
    }

    CHECK(touched == 3);
    CHECK(*table.get("x") == 11);
    CHECK(*table.get("y") == 12);
    CHECK(*table.get("z") == 13);
}
