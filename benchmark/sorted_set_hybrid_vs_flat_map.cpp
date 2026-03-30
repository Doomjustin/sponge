#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <boost/unordered/unordered_flat_map.hpp>

#include <sponge/hash.h>
#include <sponge/redis/sorted_set.h>

namespace {

using Clock = std::chrono::high_resolution_clock;

struct FlatMapOnlyZSet {
    using Score = double;
    using String = std::pmr::string;
    using Dict = boost::unordered_flat_map<String,
                                           Score,
                                           spg::PmrStringHash,
                                           std::equal_to<>,
                                           std::pmr::polymorphic_allocator<std::pair<const String, Score>>>;

    explicit FlatMapOnlyZSet(std::pmr::memory_resource* resource)
      : dict{ resource }
    {}

    auto add(Score score, std::string_view member) -> bool
    {
        auto [it, inserted] = dict.try_emplace(String{ member, dict.get_allocator().resource() }, score);
        if (!inserted)
            it->second = score;

        return inserted;
    }

    auto score(std::string_view member) const -> std::optional<Score>
    {
        auto it = dict.find(member);
        if (it == dict.end())
            return std::nullopt;

        return it->second;
    }

    auto erase(std::string_view member) -> bool
    {
        return dict.erase(member) > 0;
    }

    auto zcount(Score min, Score max) const -> size_t
    {
        size_t count = 0;
        for (const auto& [member, score] : dict) {
            if (score >= min && score <= max)
                ++count;
        }

        return count;
    }

    auto zrank(std::string_view member) const -> std::optional<int64_t>
    {
        auto it = dict.find(member);
        if (it == dict.end())
            return std::nullopt;

        const auto target_score = it->second;
        int64_t rank = 0;
        for (const auto& [candidate_member, candidate_score] : dict) {
            if (candidate_score < target_score ||
                (candidate_score == target_score && candidate_member < member)) {
                ++rank;
            }
        }

        return rank;
    }

private:
    Dict dict;
};

template<typename Func>
auto measure_ms(Func&& func) -> double
{
    auto begin = Clock::now();
    func();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin);
    return static_cast<double>(elapsed.count()) / 1000.0;
}

auto make_members(size_t count) -> std::vector<std::string>
{
    std::vector<std::string> members;
    members.reserve(count);
    for (size_t i = 0; i < count; ++i)
        members.emplace_back("member:" + std::to_string(i));

    return members;
}

auto make_scores(size_t count, std::mt19937_64& rng) -> std::vector<double>
{
    std::uniform_real_distribution<double> dist{ 0.0, 1'000'000.0 };
    std::vector<double> scores;
    scores.reserve(count);
    for (size_t i = 0; i < count; ++i)
        scores.push_back(dist(rng));

    return scores;
}

template<typename ZSet>
auto bench_zadd(ZSet& set, const std::vector<std::string>& members, const std::vector<double>& scores) -> double
{
    return measure_ms([&]() {
        for (size_t i = 0; i < members.size(); ++i)
            set.add(scores[i], members[i]);
    });
}

template<typename ZSet>
auto bench_zscore(ZSet& set, const std::vector<std::string>& members, std::mt19937_64& rng, size_t rounds) -> double
{
    std::uniform_int_distribution<size_t> pick{ 0, members.size() - 1 };
    volatile double sink = 0.0;
    return measure_ms([&]() {
        for (size_t i = 0; i < rounds; ++i) {
            auto idx = pick(rng);
            if (auto s = set.score(members[idx]))
                sink += *s;
        }
    });
}

template<typename ZSet>
auto bench_zcount(ZSet& set, std::mt19937_64& rng, size_t rounds) -> double
{
    std::uniform_real_distribution<double> base{ 0.0, 900'000.0 };
    volatile size_t sink = 0;
    return measure_ms([&]() {
        for (size_t i = 0; i < rounds; ++i) {
            auto lo = base(rng);
            auto hi = lo + 100'000.0;
            if constexpr (requires { set.zcount(lo, hi); })
                sink += set.zcount(lo, hi);
            else {
                size_t count = 0;
                for (auto it = set.begin(); it != set.end(); ++it) {
                    auto [member, score] = *it;
                    if (score >= lo && score <= hi)
                        ++count;
                }
                sink += count;
            }
        }
    });
}

template<typename ZSet>
auto bench_zrank(ZSet& set, const std::vector<std::string>& members, std::mt19937_64& rng, size_t rounds) -> double
{
    std::uniform_int_distribution<size_t> pick{ 0, members.size() - 1 };
    volatile int64_t sink = 0;
    return measure_ms([&]() {
        for (size_t i = 0; i < rounds; ++i) {
            auto idx = pick(rng);
            if constexpr (requires { set.zrank(members[idx]); }) {
                if (auto rank = set.zrank(members[idx]))
                    sink += *rank;
            }
            else {
                int64_t rank = 0;
                bool found = false;
                for (auto it = set.begin(); it != set.end(); ++it, ++rank) {
                    auto [member, score] = *it;
                    if (std::string_view{ member } == members[idx]) {
                        sink += rank;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    sink += -1;
            }
        }
    });
}

template<typename ZSet>
auto bench_zrem(ZSet& set, const std::vector<std::string>& members) -> double
{
    return measure_ms([&]() {
        for (const auto& m : members)
            set.erase(m);
    });
}

void print_row(std::string_view op, double flat_ms, double hybrid_ms)
{
    auto ratio = flat_ms / hybrid_ms;
    std::cout << std::left << std::setw(10) << op
              << std::right << std::setw(16) << std::fixed << std::setprecision(2) << flat_ms
              << std::setw(16) << std::fixed << std::setprecision(2) << hybrid_ms
              << std::setw(16) << std::fixed << std::setprecision(2) << ratio << "x\n";
}

} // namespace

int main()
{
    constexpr size_t element_count = 200'000;
    constexpr size_t zscore_rounds = 200'000;
    constexpr size_t ordered_query_rounds = 1'000;

    std::mt19937_64 rng{ 42 };
    auto members = make_members(element_count);
    auto scores = make_scores(element_count, rng);

    std::cout << "SortedSet Benchmark: unordered_flat_map-only vs hybrid(listpack+skiplist)\n";
    std::cout << "elements=" << element_count
              << ", zscore_rounds=" << zscore_rounds
              << ", ordered_query_rounds=" << ordered_query_rounds << "\n\n";
    std::cout << std::left << std::setw(10) << "op"
              << std::right << std::setw(16) << "flat_map(ms)"
              << std::setw(16) << "hybrid(ms)"
              << std::setw(16) << "flat/hybrid" << "\n";
    std::cout << std::string(58, '-') << "\n";

    std::pmr::monotonic_buffer_resource flat_mbr;
    FlatMapOnlyZSet flat{ &flat_mbr };
    spg::redis::SortedSet hybrid;

    auto zadd_flat = bench_zadd(flat, members, scores);
    auto zadd_hybrid = bench_zadd(hybrid, members, scores);
    print_row("zadd", zadd_flat, zadd_hybrid);

    auto zscore_flat = bench_zscore(flat, members, rng, zscore_rounds);
    auto zscore_hybrid = bench_zscore(hybrid, members, rng, zscore_rounds);
    print_row("zscore", zscore_flat, zscore_hybrid);

    auto zcount_flat = bench_zcount(flat, rng, ordered_query_rounds);
    auto zcount_hybrid = bench_zcount(hybrid, rng, ordered_query_rounds);
    print_row("zcount", zcount_flat, zcount_hybrid);

    auto zrank_flat = bench_zrank(flat, members, rng, ordered_query_rounds);
    auto zrank_hybrid = bench_zrank(hybrid, members, rng, ordered_query_rounds);
    print_row("zrank", zrank_flat, zrank_hybrid);

    auto zrem_flat = bench_zrem(flat, members);
    auto zrem_hybrid = bench_zrem(hybrid, members);
    print_row("zrem", zrem_flat, zrem_hybrid);

    return 0;
}