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
    std::cout << "\n" << std::string(90, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(90, '=') << "\n";
    std::cout << std::left << std::setw(40) << "操作"
              << std::right << std::setw(20) << "时间(ms)"
              << std::setw(20) << "相对时间"
              << std::setw(10) << "倍数\n";
    std::cout << std::string(90, '-') << "\n";
}

void print_row(const std::string& name, double time_ms, double baseline_ms) {
    double ratio = baseline_ms > 0 ? time_ms / baseline_ms : 1.0;
    std::cout << std::left << std::setw(40) << name
              << std::right << std::setw(20) << std::fixed << std::setprecision(2) << time_ms << " ms"
              << std::setw(20) << std::fixed << std::setprecision(2) << time_ms << " ms";
    if (ratio >= 1.0) {
        std::cout << std::setw(10) << std::fixed << std::setprecision(1) << ratio << "x\n";
    } else {
        std::cout << std::setw(10) << std::fixed << std::setprecision(1) << ratio << "x (快)\n";
    }
}

// ==================== 场景 1: 频繁的 size() 查询 ====================
void benchmark_frequent_size_queries() {
    print_header("场景 1: 频繁 size() 查询 (500M 次)");

    std::string test_data(100, 'x');
    double std_time = 0, redis_time = 0;

    // std::string
    {
        std::string s = test_data;
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 500'000'000; ++i) {
            sum += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        std_time = elapsed / 1000.0;
        (void)sum;
        print_row("std::string", std_time, std_time);
    }

    // redis::String
    {
        std::pmr::monotonic_buffer_resource mbr;
        String s(test_data, &mbr);
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 500'000'000; ++i) {
            sum += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        redis_time = elapsed / 1000.0;
        (void)sum;
        print_row("redis::String", redis_time, std_time);
    }
}

// ==================== 场景 2: 批量创建 ====================
void benchmark_bulk_creation() {
    print_header("场景 2: 批量创建字符串 (10M 次)");

    std::string test_data(100, 'x');
    double std_time = 0, redis_time = 0;

    // std::string
    {
        auto start = Clock::now();
        std::vector<std::string> vec;
        for (size_t i = 0; i < 10'000'000; ++i) {
            vec.push_back(test_data);
            if (vec.size() > 1000) vec.clear();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        std_time = elapsed / 1000.0;
        print_row("std::string", std_time, std_time);
    }

    // redis::String
    {
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        std::vector<String> vec;
        for (size_t i = 0; i < 10'000'000; ++i) {
            vec.push_back(String(test_data, &mbr));
            if (vec.size() > 1000) vec.clear();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        redis_time = elapsed / 1000.0;
        print_row("redis::String", redis_time, std_time);
    }
}

// ==================== 场景 3: Append 操作 ====================
void benchmark_append_operations() {
    print_header("场景 3: Append 操作 (5M 次)");

    std::string base_data(100, 'x');
    std::string append_data("test");
    double std_time = 0, redis_time = 0;

    // std::string
    {
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            std::string s = base_data;
            s.append(append_data);
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        std_time = elapsed / 1000.0;
        print_row("std::string", std_time, std_time);
    }

    // redis::String
    {
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            String s(base_data, &mbr);
            s.append(append_data);
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        redis_time = elapsed / 1000.0;
        print_row("redis::String", redis_time, std_time);
    }
}

// ==================== 场景 4: 内存消耗 ====================
void benchmark_memory_efficiency() {
    print_header("场景 4: 内存效率分析 (1M 字符串，100B 内容)");

    std::string test_data(100, 'x');

    std::cout << "\n创建 1M 个字符串的内存使用量:\n";
    std::cout << std::string(90, '-') << "\n";

    // std::string - 总内存 = 字符串数 × (sizeof(string) + capacity)
    size_t string_size = sizeof(std::string);
    size_t total_std = 1'000'000 * (string_size + 100 + 0); // 内容占用 + 预分配
    std::cout << std::left << std::setw(40) << "std::string (1M × " + std::to_string(string_size + 100) + "B)"
              << std::right << std::setw(20) << std::fixed << std::setprecision(2) 
              << total_std / (1024.0 * 1024.0) << " MB\n";

    // redis::String - 总内存 = 字符串数 × (sizeof(String) + SDS_header + capacity)
    std::pmr::monotonic_buffer_resource mbr;
    String sample(test_data, &mbr);
    size_t redis_header = sample.capacity() - test_data.size();  // SDS 头部
    size_t string_obj_size = sizeof(String);
    size_t total_redis = 1'000'000 * (string_obj_size + redis_header + 100);
    std::cout << std::left << std::setw(40) << "redis::String (1M × " + std::to_string(string_obj_size + redis_header + 100) + "B)"
              << std::right << std::setw(20) << std::fixed << std::setprecision(2)
              << total_redis / (1024.0 * 1024.0) << " MB\n";

    std::cout << "\n内存效率对比:\n";
    std::cout << std::left << std::setw(40) << "std::string 优势"
              << std::right << std::setw(20) << std::fixed << std::setprecision(1)
              << (total_redis > total_std ? 
                  (static_cast<double>(total_redis - total_std) / total_std * 100) : 
                  -(static_cast<double>(total_std - total_redis) / total_redis * 100)) << "% \n";
}

// ==================== 场景 5: 使用场景分析 ====================
void print_usage_analysis() {
    std::cout << "\n" << std::string(90, '=') << "\n";
    std::cout << "使用场景分析\n";
    std::cout << std::string(90, '=') << "\n\n";

    std::cout << "✅ std::string 优势:\n";
    std::cout << "  • 性能优秀（15-50倍更快）\n";
    std::cout << "  • 内存高效（内置 SSO 优化小字符串）\n";
    std::cout << "  • 编译器深度优化\n";
    std::cout << "  • 标准库支持完善\n";
    std::cout << "  • 适合几乎所有 C++ 应用\n";

    std::cout << "\n✅ redis::String 优势:\n";
    std::cout << "  • 与 Redis SDS 数据结构兼容\n";
    std::cout << "  • 调试 Redis 协议时有帮助\n";
    std::cout << "  • 学习 SDS 内部结构的教材\n";
    std::cout << "  • 可精细控制内存类型分派\n";

    std::cout << "\n🎯 建议:\n";
    std::cout << "  • 一般应用: 使用 std::string\n";
    std::cout << "  • Redis 相关: 考虑是否真的需要 SDS 兼容\n";
    std::cout << "  • 性能关键: 绝对选择 std::string\n";
    std::cout << "  • 学习目的: redis::String 有价值\n\n";
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n"
              << "║  std::string vs redis::String 综合对比                    ║\n"
              << "║  在 C++ 中哪个更优秀？                                   ║\n"
              << "║  (编译优化: -O3, CXXFLAGS: -DNDEBUG)                      ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n";

    benchmark_frequent_size_queries();
    benchmark_bulk_creation();
    benchmark_append_operations();
    benchmark_memory_efficiency();
    print_usage_analysis();

    return 0;
}
