#include "sponge/random.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

using namespace spg;

TEST_CASE("random uniform integral stays in half-open interval", "[spg_base_random]")
{
    random::seed(12345U);

    for (int i = 0; i < 200; ++i) {
        auto value = random::uniform(10, 20);
        REQUIRE(value >= 10);
        REQUIRE(value < 20);
    }
}

TEST_CASE("random uniform floating stays in interval", "[spg_base_random]")
{
    random::seed(67890U);

    for (int i = 0; i < 200; ++i) {
        auto value = random::uniform(0.0, 1.0);
        REQUIRE(value >= 0.0);
        REQUIRE(value < 1.0);
    }
}

TEST_CASE("random seeded sequence is deterministic", "[spg_base_random]")
{
    random::seed(42U);
    std::array<int, 8> first{};
    for (auto& v : first)
        v = random::uniform(1000);

    random::seed(42U);
    std::array<int, 8> second{};
    for (auto& v : second)
        v = random::uniform(1000);

    REQUIRE(first == second);
}

TEST_CASE("random bernoulli and binomial output valid ranges", "[spg_base_random]")
{
    random::seed(24680U);

    for (int i = 0; i < 100; ++i) {
        auto b = random::bernoulli(0.3);
        REQUIRE((b == true || b == false));

        auto n = random::binomial(10, 0.5);
        REQUIRE(n >= 0);
        REQUIRE(n <= 10);
    }
}

TEST_CASE("random normal and exponential produce finite values", "[spg_base_random]")
{
    random::seed(13579U);

    for (int i = 0; i < 100; ++i) {
        auto normal = random::normal(0.0, 1.0);
        auto expo = random::exponential(1.0);

        REQUIRE(std::isfinite(normal));
        REQUIRE(std::isfinite(expo));
        REQUIRE(expo >= 0.0);
    }
}

TEST_CASE("random shuffle preserves elements", "[spg_base_random]")
{
    random::seed(112233U);

    std::vector<int> values(20);
    std::ranges::iota(values, 0);
    auto original = values;

    random::shuffle(values);

    std::ranges::sort(values);
    REQUIRE(values == original);
}

TEST_CASE("random choice and sample respect source set", "[spg_base_random]")
{
    random::seed(556677U);

    const std::vector<int> values{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    auto chosen = random::choice(values);
    REQUIRE(std::ranges::find(values, chosen) != values.end());

    auto sampled = random::sample(values, 4U);
    REQUIRE(sampled.size() == 4);

    for (auto v : sampled)
        REQUIRE(std::ranges::find(values, v) != values.end());
}

TEST_CASE("random thread_local engine gives same sequence with same seed", "[spg_base_random]")
{
    std::array<int, 8> seq_a{};
    std::array<int, 8> seq_b{};

    auto task_a = [&seq_a] -> void 
    {
        random::seed(424242U);
        for (auto& v : seq_a)
            v = random::uniform(1000000);
    };

    auto task_b = [&seq_b] -> void 
    {
        random::seed(424242U);
        for (auto& v : seq_b)
            v = random::uniform(1000000);
    };

    std::thread t1{ task_a };
    std::thread t2{ task_b };

    t1.join();
    t2.join();

    REQUIRE(seq_a == seq_b);
}

TEST_CASE("random child thread does not affect main thread engine state", "[spg_base_random]")
{
    random::seed(777U);
    auto first = random::uniform(1000000);

    auto bg_task = [] -> void
    {
        random::seed(123U);
        random::uniform(1000000);
        random::uniform(1000000);
    };

    std::thread worker{ bg_task };
    worker.join();

    auto second = random::uniform(1000000);

    random::seed(777U);
    auto expected_first = random::uniform(1000000);
    auto expected_second = random::uniform(1000000);

    REQUIRE(first == expected_first);
    REQUIRE(second == expected_second);
}