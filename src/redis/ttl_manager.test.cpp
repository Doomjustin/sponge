#include <sponge/redis/ttl_manager.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

TEST_CASE("TTLManager identifies persistent time point", "[spg_redis_ttl_manager][persist]")
{
    REQUIRE(TTLManager::is_persist(TTLManager::TimePoint::max()));
    REQUIRE_FALSE(TTLManager::is_persist(TTLManager::Clock::now()));
}

TEST_CASE("TTLManager maps negative ttl to persistent sentinel",
          "[spg_redis_ttl_manager][expire_at_cast]")
{
    REQUIRE(TTLManager::expire_at(TTLManager::Milliseconds{ -1 }, 
            spg::redis::return_integral) == -1);
}

TEST_CASE("TTLManager converts positive ttl to future time point",
          "[spg_redis_ttl_manager][expire_at_cast]")
{
    constexpr auto ttl{ TTLManager::Milliseconds{ 50 } };
    auto before{ TTLManager::Clock::now() };

    auto expire_at{ TTLManager::expire_at(ttl) };

    auto after{ TTLManager::Clock::now() };

    REQUIRE(expire_at >= before + ttl);
    REQUIRE(expire_at <= after + ttl);
}

TEST_CASE("TTLManager converts positive ttl to future integral expiration time",
          "[spg_redis_ttl_manager][expire_at_cast]")
{
    constexpr auto ttl{ TTLManager::Milliseconds{ 50 } };
    auto before{ std::chrono::duration_cast<TTLManager::Milliseconds>(
                     TTLManager::Clock::now().time_since_epoch()).count() };

    auto expire_at{ TTLManager::expire_at(ttl, spg::redis::return_integral) };

    auto after{ std::chrono::duration_cast<TTLManager::Milliseconds>(
                    TTLManager::Clock::now().time_since_epoch()).count() };

    REQUIRE(expire_at >= before + ttl.count());
    REQUIRE(expire_at <= after + ttl.count());
}