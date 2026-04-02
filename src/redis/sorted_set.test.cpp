#include <sponge/redis/sorted_set.h>

#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sorted_set迭代器应返回成员与分数对", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    REQUIRE(set.add(1.5, "a"));
    REQUIRE(set.add(2.5, "b"));

    std::vector<std::pair<std::string, double>> items;
    for (auto it = set.begin(); it != set.end(); ++it) {
        auto [member, score] = *it;
        items.emplace_back(std::string{ member }, score);
    }

    REQUIRE(items.size() == 2);
    REQUIRE(items[0].first == "a");
    REQUIRE(items[0].second == 1.5);
    REQUIRE(items[1].first == "b");
    REQUIRE(items[1].second == 2.5);
}

TEST_CASE("sorted_set更新已有成员分数时应不产生重复成员", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    REQUIRE(set.add(3.0, "a"));
    REQUIRE_FALSE(set.add(1.0, "a"));

    auto s = set.score("a");
    REQUIRE(s.has_value());
    REQUIRE(*s == 1.0);

    std::vector<std::pair<std::string, double>> items;
    for (auto it = set.begin(); it != set.end(); ++it) {
        auto [member, score] = *it;
        items.emplace_back(std::string{ member }, score);
    }

    REQUIRE(items.size() == 1);
    REQUIRE(items[0].first == "a");
    REQUIRE(items[0].second == 1.0);
}

TEST_CASE("sorted_set在listpack后端保持数字成员为字符串", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    REQUIRE(set.add(2.0, "2"));
    REQUIRE(set.add(1.0, "10"));

    std::vector<std::pair<std::string, double>> items;
    for (auto it = set.begin(); it != set.end(); ++it) {
        auto [member, score] = *it;
        items.emplace_back(member, score);
    }

    REQUIRE(items.size() == 2);
    REQUIRE(items[0] == std::pair<std::string, double>{ "10", 1.0 });
    REQUIRE(items[1] == std::pair<std::string, double>{ "2", 2.0 });
}

TEST_CASE("sorted_set升级到skiplist后保持迭代行为", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    for (int i = 0; i < 130; ++i)
        REQUIRE(set.add(static_cast<double>(130 - i), std::to_string(i)));

    size_t count = 0;
    double last_score = -1.0;
    for (auto it = set.begin(); it != set.end(); ++it) {
        auto [member, score] = *it;
        REQUIRE(score >= last_score);
        last_score = score;
        ++count;
    }

    REQUIRE(count == 130);
}

TEST_CASE("sorted_set同分成员按字典序排序", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    REQUIRE(set.add(1.0, "c"));
    REQUIRE(set.add(1.0, "a"));
    REQUIRE(set.add(1.0, "b"));

    std::vector<std::string> members;
    for (auto it = set.begin(); it != set.end(); ++it) {
        auto [member, score] = *it;
        REQUIRE(score == 1.0);
        members.emplace_back(member);
    }

    REQUIRE(members == std::vector<std::string>{ "a", "b", "c" });
}

TEST_CASE("sorted_set升级后contains应保持正确", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    for (int i = 0; i < 130; ++i)
        REQUIRE(set.add(static_cast<double>(i), std::to_string(i)));

    REQUIRE(set.contains("0"));
    REQUIRE(set.contains("64"));
    REQUIRE(set.contains("129"));
    REQUIRE_FALSE(set.contains("130"));
}

TEST_CASE("sorted_set升级后score应保持正确", "[redis][sorted_set]")
{
    spg::redis::SortedSet set;
    for (int i = 0; i < 130; ++i)
        REQUIRE(set.add(static_cast<double>(i), std::to_string(i)));

    auto zero = set.score("0");
    auto middle = set.score("64");
    auto last = set.score("129");
    auto missing = set.score("130");

    REQUIRE(zero.has_value());
    REQUIRE(*zero == 0.0);
    REQUIRE(middle.has_value());
    REQUIRE(*middle == 64.0);
    REQUIRE(last.has_value());
    REQUIRE(*last == 129.0);
    REQUIRE_FALSE(missing.has_value());
}

TEST_CASE("sorted_set支持常量迭代", "[redis][sorted_set]")
{
    spg::redis::SortedSet mutable_set;
    REQUIRE(mutable_set.add(2.0, "b"));
    REQUIRE(mutable_set.add(1.0, "a"));

    const auto& set = mutable_set;
    std::vector<std::pair<std::string, double>> items;
    for (auto it = set.begin(); it != set.end(); ++it) {
        auto [member, score] = *it;
        items.emplace_back(member, score);
    }

    REQUIRE(items.size() == 2);
    REQUIRE(items[0] == std::pair<std::string, double>{ "a", 1.0 });
    REQUIRE(items[1] == std::pair<std::string, double>{ "b", 2.0 });
}
