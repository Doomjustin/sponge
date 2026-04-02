#include "db_shard.h"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

using spg::redis::DBShard;
using spg::redis::TTLManager;
using spg::as_type;

TEST_CASE("db_shard set/get/erase条目", "[redis][db_shard]")
{
    DBShard shard;

    DBShard::Entry entry{
        .value = DBShard::String{ "hello" },
        .expire_at = TTLManager::PERSISTENT
    };

    shard.set("k", std::move(entry));

    auto* found = shard.get_entry("k");
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<DBShard::String>(found->value));
    CHECK(std::get<DBShard::String>(found->value) == "hello");

    CHECK(shard.erase("k"));
    CHECK_FALSE(shard.erase("k"));
    CHECK(shard.get_entry("k") == nullptr);
}

TEST_CASE("db_shard modify应支持创建新值", "[redis][db_shard]")
{
    DBShard shard;

    shard.modify("fresh", [](auto& handler) {
        REQUIRE_FALSE(handler.exists());
        auto* value = handler.emplace(as_type<DBShard::String>, "v");
        REQUIRE(value != nullptr);
        CHECK(*value == "v");
    });

    shard.modify("fresh", [](auto& handler) {
        REQUIRE(handler.exists());
        auto* value = handler.get_if(as_type<DBShard::String>);
        REQUIRE(value != nullptr);
        CHECK(*value == "v");
    });
}

TEST_CASE("db_shard modify应清理已过期键", "[redis][db_shard]")
{
    DBShard shard;

    DBShard::Entry expired{
        .value = DBShard::String{ "old" },
        .expire_at = TTLManager::now() - 1
    };
    shard.set("expired", std::move(expired));

    shard.modify("expired", [](auto& handler) {
        CHECK_FALSE(handler.exists());
    });

    CHECK(shard.get_entry("expired") == nullptr);
}

TEST_CASE("db_shard modify应支持rename", "[redis][db_shard]")
{
    DBShard shard;

    shard.modify("old_key", [](auto& handler) {
        auto* value = handler.emplace(as_type<DBShard::String>, "payload");
        REQUIRE(value != nullptr);
    });

    shard.modify("old_key", [](auto& handler) {
        REQUIRE(handler.exists());
        CHECK(handler.rename("new_key"));

        auto* value = handler.get_if(as_type<DBShard::String>);
        REQUIRE(value != nullptr);
        CHECK(*value == "payload");
    });

    CHECK(shard.get_entry("old_key") == nullptr);
    auto* renamed = shard.get_entry("new_key");
    REQUIRE(renamed != nullptr);
    REQUIRE(std::holds_alternative<DBShard::String>(renamed->value));
}

TEST_CASE("db_shard modify应支持erase", "[redis][db_shard]")
{
    DBShard shard;

    shard.modify("old_key", [](auto& handler) {
        auto* value = handler.emplace(as_type<DBShard::String>, "payload");
        REQUIRE(value != nullptr);
    });

    shard.modify("old_key", [](auto& handler) {
        REQUIRE(handler.exists());

        CHECK(handler.erase());
        CHECK_FALSE(handler.exists());
        CHECK_FALSE(handler.erase());
    });

    CHECK(shard.get_entry("old_key") == nullptr);
}

TEST_CASE("db_shard modify应支持expire并返回TTL", "[redis][db_shard]")
{
    DBShard shard;

    shard.modify("ttl_key", [](auto& handler) {
        auto* value = handler.emplace(as_type<DBShard::Integral>, 42);
        REQUIRE(value != nullptr);
        CHECK(*value == 42);
        CHECK(handler.expire(1000));
    });

    shard.modify("ttl_key", [](auto& handler) {
        REQUIRE(handler.exists());

        auto ttl = handler.ttl();
        REQUIRE(ttl.has_value());
        CHECK(*ttl > 0);
    });
}

TEST_CASE("db_shard modify应支持persist", "[redis][db_shard]")
{
    DBShard shard;

    shard.modify("ttl_key", [](auto& handler) {
        auto* value = handler.emplace(as_type<DBShard::Integral>, 42);
        REQUIRE(value != nullptr);
        CHECK(handler.expire(1000));
    });

    shard.modify("ttl_key", [](auto& handler) {
        REQUIRE(handler.exists());
        CHECK(handler.persist());

        auto persistent_ttl = handler.ttl();
        REQUIRE(persistent_ttl.has_value());
        CHECK(*persistent_ttl == TTLManager::PERSISTENT);
    });
}
