#include <sponge/hash.h>

#include <string>
#include <string_view>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("hash 对相同输入应返回稳定哈希值", "[common][hash]")
{
    REQUIRE(hash("hello", use_std) == hash("hello", use_std));
}

TEST_CASE("hash 对不同输入应返回不同哈希值", "[common][hash]")
{
    REQUIRE(hash("foo", use_std) != hash("bar", use_std));
}

TEST_CASE("StringHash 应支持 unordered_map 异构查找", "[common][hash]")
{
    std::unordered_map<std::string, int, StringHash, std::equal_to<>> map{};
    map.emplace("key", 42);

    std::string_view sv{ "key" };
    auto it{ map.find(sv) };

    REQUIRE(it != map.end());
    REQUIRE(it->second == 42);
}

TEST_CASE("PmrStringHash 应支持 unordered_map 异构查找", "[common][hash]")
{
    std::pmr::unordered_map<std::pmr::string, int, PmrStringHash, std::equal_to<>> map{};
    map.emplace("key", 99);

    std::string_view sv{ "key" };
    auto it{ map.find(sv) };

    REQUIRE(it != map.end());
    REQUIRE(it->second == 99);
}

TEST_CASE("hash 应正确处理空字符串", "[common][hash]")
{
    auto hash1 = hash("", use_std);
    auto hash2 = hash("", use_std);

    REQUIRE(hash1 == hash2);
    REQUIRE(hash1 != hash("a", use_std));
}

TEST_CASE("hash 在不同算法下应保持各自结果稳定", "[common][hash]")
{
    const std::string test_strings[] = { "test", "hello", "world", "123", "" };

    for (const auto& s : test_strings) {
        auto hash_std = hash(s, use_std);
        auto hash_fnv = hash(s, use_fnv_1a);

        // Same string with same algorithm should produce same hash
        REQUIRE(hash(s, use_std) == hash_std);
        REQUIRE(hash(s, use_fnv_1a) == hash_fnv);
    }
}

TEST_CASE("hash 应区分相似的字符串", "[common][hash]")
{
    // Strings with small differences should (likely) have different hashes
    auto h1 = hash("abc", use_std);
    auto h2 = hash("abd", use_std);
    auto h3 = hash("bca", use_std);

    REQUIRE(h1 != h2);
    REQUIRE(h1 != h3);
    REQUIRE(h2 != h3);
}

TEST_CASE("StringHash 在多键场景下应支持快速查找", "[common][hash]")
{
    std::unordered_map<std::string, int, StringHash, std::equal_to<>> map{};

    const int test_data[] = { 10, 20, 30, 40, 50 };
    const std::string keys[] = { "alpha", "beta", "gamma", "delta", "epsilon" };

    for (std::size_t i = 0; i < 5; ++i)
        map.emplace(keys[i], test_data[i]);

    // Test lookups with string_view
    for (std::size_t i = 0; i < 5; ++i) {
        std::string_view sv{ keys[i] };
        auto it{ map.find(sv) };
        REQUIRE(it != map.end());
        REQUIRE(it->second == test_data[i]);
    }

    // Test failed lookups
    std::string_view missing{ "theta" };
    REQUIRE(map.find(missing) == map.end());
}

TEST_CASE("PmrStringHash 在多键场景下应支持快速查找", "[common][hash]")
{
    std::pmr::unordered_map<std::pmr::string, int, PmrStringHash, std::equal_to<>> map{};

    const std::pmr::string keys[] = {
        std::pmr::string{ "key1" },
        std::pmr::string{ "key2" },
        std::pmr::string{ "key3" }
    };

    for (std::size_t i = 0; i < 3; ++i)
        map.emplace(keys[i], static_cast<int>(i));

    // Lookup with string_view
    std::string_view sv{ "key2" };
    auto it{ map.find(sv) };
    REQUIRE(it != map.end());
    REQUIRE(it->second == 1);
}
