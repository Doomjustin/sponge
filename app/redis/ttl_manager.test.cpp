#include "ttl_manager.h"

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

TEST_CASE("TTLManager识别持久时间点时应返回正确判断", "[redis][ttl_manager]")
{
    REQUIRE(TTLManager::is_persist(TTLManager::TimePoint::max()));
    REQUIRE_FALSE(TTLManager::is_persist(TTLManager::Clock::now()));
}

TEST_CASE("TTLManager将负TTL映射为持久哨兵值",
          "[redis][ttl_manager]")
{
    REQUIRE(TTLManager::expire_at(-1) == -1);
}

TEST_CASE("TTLManager将正TTL转换为未来时间点", "[redis][ttl_manager]")
{
    constexpr auto ttl{ TTLManager::Milliseconds{ 50 } };
    auto before{ TTLManager::Clock::now() };

    auto expire_at{ TTLManager::expire_at(ttl) };

    auto after{ TTLManager::Clock::now() };

    REQUIRE(expire_at >= before + ttl);
    REQUIRE(expire_at <= after + ttl);
}

TEST_CASE("TTLManager将正TTL转换为未来整数过期时间", "[redis][ttl_manager]")
{
    constexpr auto ttl{ TTLManager::Milliseconds{ 50 } };
    auto before{ std::chrono::duration_cast<TTLManager::Milliseconds>(
                     TTLManager::Clock::now().time_since_epoch()).count() };

    auto expire_at{ TTLManager::expire_at(ttl.count()) };

    auto after{ std::chrono::duration_cast<TTLManager::Milliseconds>(
                    TTLManager::Clock::now().time_since_epoch()).count() };

    REQUIRE(expire_at >= before + ttl.count());
    REQUIRE(expire_at <= after + ttl.count());
}
