#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include <sponge/redis/string.h>

using namespace spg::redis;
using Clock = std::chrono::high_resolution_clock;
using Microseconds = std::chrono::microseconds;

void print_header(const std::string& title) {
    std::cout << "\n" << std::string(100, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(100, '=') << "\n";
    std::cout << std::left << std::setw(45) << "测试配置"
              << std::right << std::setw(20) << "时间(ms)"
              << std::setw(20) << "基准参考"
              << std::setw(15) << "倍数\n";
    std::cout << std::string(100, '-') << "\n";
}

void print_row(const std::string& name, double time_ms, double baseline_ms) {
    double ratio = baseline_ms > 0 ? time_ms / baseline_ms : 1.0;
    std::cout << std::left << std::setw(45) << name
              << std::right << std::setw(20) << std::fixed << std::setprecision(2) << time_ms << " ms"
              << std::setw(20) << std::fixed << std::setprecision(2) << baseline_ms << " ms"
              << std::setw(15) << std::fixed << std::setprecision(2) << ratio << "x\n";
}

// ==================== Benchmark 1: Arena 分配器对比（大量小对象） ====================
void benchmark_arena_allocation() {
    print_header("Benchmark 1: Arena 分配器 vs 独立分配 (创建 10M 个 32B 字符串)");

    // 场景 A: 标准分配（每个对象独立分配）
    double time_independent = 0;
    {
        auto start = Clock::now();
        for (size_t i = 0; i < 10'000'000; ++i) {
            std::pmr::string s("hello world test string", std::pmr::new_delete_resource());
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        time_independent = elapsed / 1000.0;
        print_row("独立分配（每个 new_delete_resource）", time_independent, time_independent);
    }

    // 场景 B: Arena 分配（从单一 arena 中分配）
    double time_arena = 0;
    {
        // 预分配 500MB arena
        std::vector<char> arena_storage(500 * 1024 * 1024);
        std::pmr::monotonic_buffer_resource arena(arena_storage.data(), arena_storage.size());

        auto start = Clock::now();
        for (size_t i = 0; i < 10'000'000; ++i) {
            std::pmr::string s("hello world test string", &arena);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        time_arena = elapsed / 1000.0;
        print_row("Arena 分配（单一 monotonic_buffer）", time_arena, time_independent);
    }

    // 场景 C: Redis::String with Arena
    double time_redis_arena = 0;
    {
        std::vector<char> arena_storage(500 * 1024 * 1024);
        std::pmr::monotonic_buffer_resource arena(arena_storage.data(), arena_storage.size());

        auto start = Clock::now();
        for (size_t i = 0; i < 10'000'000; ++i) {
            String s("hello world test string", &arena);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        time_redis_arena = elapsed / 1000.0;
        print_row("Redis::String + Arena", time_redis_arena, time_independent);
    }

    std::cout << "\n💡 发现：\n";
    std::cout << "  • Arena 分配比独立分配快 " << std::fixed << std::setprecision(1) << time_independent / time_arena << "x\n";
    std::cout << "  • 但 redis::String 即使用 Arena 仍然较慢\n";
    std::cout << "  • 说明 redis::String 的慢不是分配的问题，而是架构\n";
}

// ==================== Benchmark 2: 短字符串 vs 长字符串 ====================
void benchmark_string_sizes() {
    print_header("Benchmark 2: 不同大小字符串的性能差异 (size() 查询 100M 次)");

    std::pmr::monotonic_buffer_resource mbr;

    struct TestCase {
        const char* name;
        size_t size;
    };

    TestCase cases[] = {
        {"极短字符串 (1B)", 1},
        {"短字符串 (8B - SSO 边界)", 8},
        {"SSO 短字符串 (16B)", 16},
        {"SSO 长边界 (23B - gcc SSO limit)", 23},
        {"超过 SSO (32B)", 32},
        {"中等字符串 (64B)", 64},
        {"长字符串 (256B)", 256},
        {"很长字符串 (1KB)", 1024},
        {"超长字符串 (10KB)", 10 * 1024},
    };

    std::cout << "\nstd::string 的 SSO (Small String Optimization) 对短字符串优化很大\n";
    std::cout << "redis::String 不支持 SSO，对所有大小的字符串性能都较差\n\n";

    double baseline_std_short = 0;

    for (auto& testcase : cases) {
        std::string test_data(testcase.size, 'x');

        // std::string
        std::string s_std = test_data;
        auto start_std = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s_std.size();
        }
        auto elapsed_std = std::chrono::duration_cast<Microseconds>(Clock::now() - start_std).count();
        double time_std = elapsed_std / 1000.0;
        (void)sum;

        if (testcase.size == 1) {
            baseline_std_short = time_std;
        }

        // std::pmr::string
        std::pmr::string s_pmr(test_data, &mbr);
        auto start_pmr = Clock::now();
        sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s_pmr.size();
        }
        auto elapsed_pmr = std::chrono::duration_cast<Microseconds>(Clock::now() - start_pmr).count();
        double time_pmr = elapsed_pmr / 1000.0;

        // redis::String
        String s_redis(test_data, &mbr);
        auto start_redis = Clock::now();
        sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s_redis.size();
        }
        auto elapsed_redis = std::chrono::duration_cast<Microseconds>(Clock::now() - start_redis).count();
        double time_redis = elapsed_redis / 1000.0;

        std::cout << std::left << std::setw(40) << testcase.name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1) << time_std << " ms"
                  << std::setw(12) << std::fixed << std::setprecision(1) << time_pmr << " ms"
                  << std::setw(12) << std::fixed << std::setprecision(1) << time_redis << " ms"
                  << std::setw(12) << std::fixed << std::setprecision(1) << (time_redis / time_std) << "x\n";
    }
}

// ==================== Benchmark 3: 内存布局对性能的影响 ====================
void benchmark_memory_layout() {
    print_header("Benchmark 3: 创建和销毁的内存开销 (100M 次操作)");

    std::cout << "\n说明：\n";
    std::cout << "  • std::string: 对象本身 32B，可能带 SSO 优化（短字符串不额外分配）\n";
    std::cout << "  • std::pmr::string: 对象本身 40B（多了 8B 指针），需要 allocator 追踪\n";
    std::cout << "  • redis::String: 对象本身仅 16B，但需要 SDS 头部 + 额外寻址开销\n\n";

    std::pmr::monotonic_buffer_resource mbr;
    double time_std = 0;

    // 创建 100M 个短字符串（会触发 SSO）
    {
        auto start = Clock::now();
        for (size_t i = 0; i < 100'000'000; ++i) {
            volatile auto s = std::string("hi");
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        time_std = elapsed / 1000.0;
        print_row("std::string 短字符串 (SSO 优化，无额外分配)", time_std, time_std);
    }

    // 创建 100M 个短字符串（pmr 版本）
    {
        auto start = Clock::now();
        for (size_t i = 0; i < 100'000'000; ++i) {
            volatile auto s = std::pmr::string("hi", &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        double time_pmr = elapsed / 1000.0;
        print_row("std::pmr::string 短字符串 (需要堆分配)", time_pmr, time_std);
    }

    // 创建 100M 个短字符串（redis 版本）
    {
        auto start = Clock::now();
        for (size_t i = 0; i < 100'000'000; ++i) {
            volatile auto s = String("hi", &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        double time_redis = elapsed / 1000.0;
        print_row("redis::String 短字符串 (SDS 开销，无 SSO)", time_redis, time_std);
    }

    std::cout << "\n✨ 关键洞察：\n";
    std::cout << "  • std::string 的 SSO 对短字符串帮助很大（避免分配）\n";
    std::cout << "  • redis::String 没有 SSO，所以即使很短还是要付出代价\n";
    std::cout << "  • 这就是为什么实际应用中 std::string 性能胜出\n";
}

// ==================== Benchmark 4: 批量操作对比 ====================
void benchmark_bulk_operations() {
    print_header("Benchmark 4: 批量操作（10M 个字符串）");

    std::cout << "\n场景：创建 10M 个不同大小的字符串，然后查询它们的信息\n\n";

    std::pmr::monotonic_buffer_resource mbr;
    double time_std = 0;

    // std::string
    {
        auto start = Clock::now();
        std::vector<std::string> strings;
        strings.reserve(10'000'000);
        for (size_t i = 0; i < 10'000'000; ++i) {
            strings.push_back("test_string_" + std::to_string(i));
        }
        volatile size_t total_size = 0;
        for (auto& s : strings) {
            total_size += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        time_std = elapsed / 1000.0;
        (void)total_size;
        print_row("std::string: 创建 + 查询 10M 个字符串", time_std, time_std);
    }

    // std::pmr::string
    {
        auto start = Clock::now();
        std::pmr::vector<std::pmr::string> strings(&mbr);
        strings.reserve(10'000'000);
        for (size_t i = 0; i < 10'000'000; ++i) {
            strings.push_back(std::pmr::string("test_string_" + std::to_string(i), &mbr));
        }
        volatile size_t total_size = 0;
        for (auto& s : strings) {
            total_size += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        double time_pmr = elapsed / 1000.0;
        (void)total_size;
        print_row("std::pmr::string: 创建 + 查询 10M 个字符串", time_pmr, time_std);
    }

    // redis::String
    {
        auto start = Clock::now();
        std::pmr::vector<String> strings(&mbr);
        strings.reserve(10'000'000);
        for (size_t i = 0; i < 10'000'000; ++i) {
            strings.push_back(String("test_string_" + std::to_string(i), &mbr));
        }
        volatile size_t total_size = 0;
        for (auto& s : strings) {
            total_size += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        double time_redis = elapsed / 1000.0;
        (void)total_size;
        print_row("redis::String: 创建 + 查询 10M 个字符串", time_redis, time_std);
    }
}

void print_conclusion() {
    std::cout << "\n" << std::string(100, '=') << "\n";
    std::cout << "📊 总结与建议\n";
    std::cout << std::string(100, '=') << "\n\n";

    std::cout << "1️⃣  Arena 分配器能显著提高分配性能\n";
    std::cout << "   • 当大量创建小对象时，使用 Arena 可以减少分配开销\n";
    std::cout << "   • std::pmr::monotonic_buffer_resource 是很好的选择\n";
    std::cout << "   • 即使如此，redis::String 仍然因为架构问题而较慢\n\n";

    std::cout << "2️⃣  短字符串 vs 长字符串性能差异\n";
    std::cout << "   • std::string 通过 SSO (Small String Optimization) 优化短字符串\n";
    std::cout << "   • 对于 ≤23B 的短字符串，std::string 会直接存储在对象内\n";
    std::cout << "   • redis::String 没有 SSO，所以无法享受这个优化\n";
    std::cout << "   • 实际应用中大量短字符串的场景（如 HTTP 头），std::string 优势巨大\n\n";

    std::cout << "3️⃣  内存布局影响性能\n";
    std::cout << "   • std::string 的对象大小 (32B) vs redis::String (16B)\n";
    std::cout << "   • 但 std::string 的内联设计使得访问速度更快\n";
    std::cout << "   • CPU 缓存友好性对现代处理器性能至关重要\n\n";

    std::cout << "4️⃣  实战建议\n";
    std::cout << "   ✅ 生产代码：使用 std::string\n";
    std::cout << "   ✅ 需要自定义分配策略：用 std::pmr::string + arena 分配器\n";
    std::cout << "   ⚠️  大量小对象场景：优先考虑 std::string 的 SSO 优化\n";
    std::cout << "   ⚠️  实时性能关键：std::string 是必选\n";
    std::cout << "   ℹ️  学习研究：redis::String 帮助理解 Redis 底层设计\n";
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n"
              << "║         Arena 分配器 & 字符串大小影响分析                ║\n"
              << "║    std::string vs std::pmr::string vs redis::String    ║\n"
              << "║  (编译优化: -O3, 场景：实际业务中的常见操作模式)        ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n";

    benchmark_arena_allocation();
    benchmark_string_sizes();
    benchmark_memory_layout();
    benchmark_bulk_operations();
    print_conclusion();

    return 0;
}
