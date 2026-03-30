#include <sponge/redis/sorted_set.h>

#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sorted_set smoke", "[sorted_set]") {
    SUCCEED();
}

TEST_CASE("sorted_set iterator returns member and score pairs", "[sorted_set]")
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

TEST_CASE("sorted_set updates existing member score without duplication", "[sorted_set]")
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

TEST_CASE("sorted_set keeps numeric members as strings in listpack backend", "[sorted_set]")
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

TEST_CASE("sorted_set keeps iterator behavior after upgrade to skiplist backend", "[sorted_set]")
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

TEST_CASE("sorted_set orders same-score members lexicographically", "[sorted_set]")
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

TEST_CASE("sorted_set contains and score remain correct after upgrade", "[sorted_set]")
{
    spg::redis::SortedSet set;
    for (int i = 0; i < 130; ++i)
        REQUIRE(set.add(static_cast<double>(i), std::to_string(i)));

    REQUIRE(set.contains("0"));
    REQUIRE(set.contains("64"));
    REQUIRE(set.contains("129"));
    REQUIRE_FALSE(set.contains("130"));

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

TEST_CASE("sorted_set supports const iteration", "[sorted_set]")
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
