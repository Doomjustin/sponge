#include <sponge/random.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("uniform 生成整数时应落在半开区间内", "[common][random]")
{
    random::seed(12345U);

    for (int i = 0; i < 200; ++i) {
        auto value = random::uniform(10, 20);
        REQUIRE(value >= 10);
        REQUIRE(value < 20);
    }
}

TEST_CASE("uniform 生成浮点数时应落在区间内", "[common][random]")
{
    random::seed(67890U);

    for (int i = 0; i < 200; ++i) {
        auto value = random::uniform(0.0, 1.0);
        REQUIRE(value >= 0.0);
        REQUIRE(value < 1.0);
    }
}

TEST_CASE("相同种子应生成确定性序列", "[common][random]")
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

TEST_CASE("bernoulli 应返回布尔值", "[common][random]")
{
    random::seed(24680U);

    for (int i = 0; i < 100; ++i) {
        auto b = random::bernoulli(0.3);
        REQUIRE((b == true || b == false));
    }
}

TEST_CASE("binomial 应返回有效范围内的值", "[common][random]")
{
    random::seed(24680U);

    for (int i = 0; i < 100; ++i) {
        auto n = random::binomial(10, 0.5);
        REQUIRE(n >= 0);
        REQUIRE(n <= 10);
    }
}

TEST_CASE("normal 应生成有限值", "[common][random]")
{
    random::seed(13579U);

    for (int i = 0; i < 100; ++i) {
        auto normal = random::normal(0.0, 1.0);
        REQUIRE(std::isfinite(normal));
    }
}

TEST_CASE("exponential 应生成非负有限值", "[common][random]")
{
    random::seed(13579U);

    for (int i = 0; i < 100; ++i) {
        auto expo = random::exponential(1.0);
        REQUIRE(std::isfinite(expo));
        REQUIRE(expo >= 0.0);
    }
}

TEST_CASE("shuffle 应保持元素集合不变", "[common][random]")
{
    random::seed(112233U);

    std::vector<int> values(20);
    std::ranges::iota(values, 0);
    auto original = values;

    random::shuffle(values);

    std::ranges::sort(values);
    REQUIRE(values == original);
}

TEST_CASE("choice 应从源集合中取值", "[common][random]")
{
    random::seed(556677U);

    const std::vector<int> values{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    auto chosen = random::choice(values);
    REQUIRE(std::ranges::find(values, chosen) != values.end());
}

TEST_CASE("sample 应返回固定数量且来自源集合的样本", "[common][random]")
{
    random::seed(556677U);

    const std::vector<int> values{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    auto sampled = random::sample(values, 4U);
    REQUIRE(sampled.size() == 4);

    for (auto v : sampled)
        REQUIRE(std::ranges::find(values, v) != values.end());
}

TEST_CASE("线程本地引擎在相同种子下应生成相同序列", "[common][random]")
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

TEST_CASE("子线程不应影响主线程的随机引擎状态", "[common][random]")
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
