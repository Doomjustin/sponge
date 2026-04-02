#include "skip_list.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("在SkipList初始化后，应该为空", "[redis][skip_list]")
{
    spg::redis::SkipList list;

    REQUIRE(list.size() == 0);
    REQUIRE(list.begin() == list.end());
    REQUIRE_FALSE(list.erase(1.0, "missing"));
    REQUIRE(list.find(1.0, "missing") == list.end());
}

TEST_CASE("SkipList按分数和元素顺序迭代时应得到有序结果", "[redis][skip_list]")
{
    spg::redis::SkipList list;
    list.insert(2.0, "b");
    list.insert(1.0, "z");
    list.insert(2.0, "a");
    list.insert(1.0, "a");
    list.insert(3.0, "x");

    std::vector<std::pair<double, std::string_view>> actual;
    for (auto it = list.begin(); it != list.end(); ++it)
        actual.push_back(*it);

    const std::vector<std::pair<double, std::string_view>> expected{
        { 1.0, "a" },
        { 1.0, "z" },
        { 2.0, "a" },
        { 2.0, "b" },
        { 3.0, "x" },
    };

    REQUIRE(list.size() == expected.size());
    REQUIRE(actual == expected);
}

TEST_CASE("SkipList执行erase时应仅删除一个匹配元素", "[redis][skip_list]")
{
    spg::redis::SkipList list;
    list.insert(1.0, "dup");
    list.insert(1.0, "dup");
    list.insert(2.0, "other");

    REQUIRE(list.size() == 3);
    REQUIRE(list.erase(1.0, "dup"));
    REQUIRE(list.size() == 2);
    REQUIRE(list.find(1.0, "dup") != list.end());

    REQUIRE(list.erase(1.0, "dup"));
    REQUIRE(list.size() == 1);
    REQUIRE(list.find(1.0, "dup") == list.end());

    REQUIRE_FALSE(list.erase(1.0, "dup"));
    REQUIRE(list.size() == 1);
}

TEST_CASE("SkipList执行update时应能重排行元素", "[redis][skip_list]")
{
    spg::redis::SkipList list;
    list.insert(1.0, "a");
    list.insert(2.0, "b");
    list.insert(3.0, "c");

    list.update(2.0, 4.0, "b");

    std::vector<std::pair<double, std::string_view>> actual;
    for (auto it = list.begin(); it != list.end(); ++it)
        actual.push_back(*it);

    const std::vector<std::pair<double, std::string_view>> expected{
        { 1.0, "a" },
        { 3.0, "c" },
        { 4.0, "b" },
    };

    REQUIRE(actual == expected);
    REQUIRE(list.find(2.0, "b") == list.end());
    REQUIRE(list.find(4.0, "b") != list.end());
}

TEST_CASE("SkipList find应返回匹配迭代器", "[redis][skip_list]")
{
    spg::redis::SkipList list;
    list.insert(10.0, "alpha");
    list.insert(20.0, "beta");

    const auto it = list.find(20.0, "beta");
    REQUIRE(it != list.end());

    const auto [score, element] = *it;
    REQUIRE(score == 20.0);
    REQUIRE(element == "beta");
}

TEST_CASE("SkipList批量插入后应保持全序", "[redis][skip_list]")
{
    spg::redis::SkipList list;

    std::vector<std::pair<double, std::string_view>> input{
        { 3.0, "k" },
        { 1.0, "c" },
        { 2.0, "a" },
        { 1.0, "a" },
        { 4.0, "x" },
        { 2.0, "b" },
        { 3.0, "a" },
        { 5.0, "z" },
        { 4.0, "b" },
        { 0.5, "m" },
    };

    for (const auto& [score, element] : input)
        list.insert(score, element);

    auto expected = input;
    std::sort(expected.begin(), expected.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second);
    });

    std::vector<std::pair<double, std::string_view>> actual;
    for (auto it = list.begin(); it != list.end(); ++it)
        actual.push_back(*it);

    REQUIRE(list.size() == input.size());
    REQUIRE(actual == expected);
}

TEST_CASE("SkipList中rank与排序位置一致", "[redis][skip_list]")
{
    spg::redis::SkipList list;

    const std::vector<std::pair<double, std::string_view>> ordered{
        { 1.0, "a" },
        { 1.0, "b" },
        { 1.0, "c" },
        { 2.0, "a" },
        { 2.0, "b" },
        { 2.0, "c" },
        { 3.0, "a" },
        { 3.0, "b" },
        { 3.0, "c" },
        { 4.0, "a" },
    };

    for (const auto& [score, element] : ordered)
        list.insert(score, element);

    for (size_t i = 0; i < ordered.size(); ++i)
        REQUIRE(list.rank(ordered[i].first, ordered[i].second) == i + 1);

    REQUIRE(list.rank(10.0, "missing") == 0);
}

TEST_CASE("SkipList反向迭代器按降序遍历", "[redis][skip_list]")
{
    spg::redis::SkipList list;
    list.insert(1.0, "a");
    list.insert(2.0, "b");
    list.insert(2.0, "a");
    list.insert(3.0, "c");

    std::vector<std::pair<double, std::string_view>> actual;
    for (auto it = list.rbegin(); it != list.rend(); ++it)
        actual.push_back(*it);

    const std::vector<std::pair<double, std::string_view>> expected{
        { 3.0, "c" },
        { 2.0, "b" },
        { 2.0, "a" },
        { 1.0, "a" },
    };

    REQUIRE(actual == expected);
}

TEST_CASE("SkipList按排名查找（1基）时应返回正确元素", "[redis][skip_list]")
{
    spg::redis::SkipList list;
    
    const std::vector<std::pair<double, std::string_view>> ordered{
        { 1.0, "a" },
        { 1.0, "b" },
        { 2.0, "a" },
        { 2.0, "b" },
        { 3.0, "a" },
    };

    for (const auto& [score, element] : ordered)
        list.insert(score, element);

    // Test each rank corresponds to correct element (1-based)
    for (size_t i = 0; i < ordered.size(); ++i) {
        const auto it = list.find(i + 1, spg::by_rank);
        REQUIRE(it != list.end());
        const auto [score, element] = *it;
        REQUIRE(score == ordered[i].first);
        REQUIRE(element == ordered[i].second);
    }

    // Rank 0 should not be found
    REQUIRE(list.find(0, spg::by_rank) == list.end());

    // Rank beyond size should not be found
    REQUIRE(list.find(list.size() + 1, spg::by_rank) == list.end());
}
