#include <algorithm>
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

constexpr size_t SMALL = 10;           // 小字符串
constexpr size_t MEDIUM = 100;         // 中等字符串
constexpr size_t LARGE = 1000;         // 大字符串

struct BenchmarkResult {
    std::string name;
    double time_ms;
    size_t iterations;
};

void print_result(const BenchmarkResult& result) {
    std::cout << std::left << std::setw(40) << result.name 
              << std::right << std::setw(12) << std::fixed << std::setprecision(3) << result.time_ms << " ms"
              << std::setw(15) << result.iterations << " iters\n";
}

// ==================== Benchmark 1: 创建字符串 ====================
void benchmark_string_creation() {
    std::cout << "\n=== Benchmark 1: 创建字符串 (10M iterations) ===\n";
    std::cout << std::left << std::setw(40) << "操作" 
              << std::right << std::setw(12) << "时间(ms)"
              << std::setw(15) << "迭代次数" << "\n"
              << std::string(67, '-') << "\n";

    // std::string - 小字符串
    {
        std::string test_str(SMALL, 'x');
        auto start = Clock::now();
        for (size_t i = 0; i < 10'000'000; ++i) {
            volatile auto s = std::string(test_str);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"std::string (小，" + std::to_string(SMALL) + " bytes)", 
                      elapsed / 1000.0, 10'000'000});
    }

    // redis::String - 小字符串
    {
        std::string test_str(SMALL, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        for (size_t i = 0; i < 10'000'000; ++i) {
            volatile auto s = String(test_str, &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"redis::String (小，" + std::to_string(SMALL) + " bytes)", 
                      elapsed / 1000.0, 10'000'000});
    }

    // std::string - 中字符串
    {
        std::string test_str(MEDIUM, 'x');
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            volatile auto s = std::string(test_str);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"std::string (中，" + std::to_string(MEDIUM) + " bytes)", 
                      elapsed / 1000.0, 5'000'000});
    }

    // redis::String - 中字符串
    {
        std::string test_str(MEDIUM, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            volatile auto s = String(test_str, &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"redis::String (中，" + std::to_string(MEDIUM) + " bytes)", 
                      elapsed / 1000.0, 5'000'000});
    }

    // std::string - 大字符串
    {
        std::string test_str(LARGE, 'x');
        auto start = Clock::now();
        for (size_t i = 0; i < 1'000'000; ++i) {
            volatile auto s = std::string(test_str);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"std::string (大，" + std::to_string(LARGE) + " bytes)", 
                      elapsed / 1000.0, 1'000'000});
    }

    // redis::String - 大字符串
    {
        std::string test_str(LARGE, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        for (size_t i = 0; i < 1'000'000; ++i) {
            volatile auto s = String(test_str, &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"redis::String (大，" + std::to_string(LARGE) + " bytes)", 
                      elapsed / 1000.0, 1'000'000});
    }
}

// ==================== Benchmark 2: Copy 操作 ====================
void benchmark_copy() {
    std::cout << "\n=== Benchmark 2: Copy 操作 (5M iterations) ===\n";
    std::cout << std::left << std::setw(40) << "操作" 
              << std::right << std::setw(12) << "时间(ms)"
              << std::setw(15) << "迭代次数" << "\n"
              << std::string(67, '-') << "\n";

    // std::string copy - 中字符串
    {
        std::string test_str(MEDIUM, 'x');
        std::vector<std::string> vec;
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            vec.push_back(test_str);
            if (vec.size() > 100) vec.clear();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"std::string copy (中，" + std::to_string(MEDIUM) + " bytes)", 
                      elapsed / 1000.0, 5'000'000});
    }

    // redis::String copy - 中字符串
    {
        std::string test_str(MEDIUM, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        String original(test_str, &mbr);
        
        auto start = Clock::now();
        std::vector<String> vec;
        for (size_t i = 0; i < 5'000'000; ++i) {
            vec.push_back(original);
            if (vec.size() > 100) vec.clear();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"redis::String copy (中，" + std::to_string(MEDIUM) + " bytes)", 
                      elapsed / 1000.0, 5'000'000});
    }
}

// ==================== Benchmark 3: Append 操作 ====================
void benchmark_append() {
    std::cout << "\n=== Benchmark 3: Append 操作 (1M iterations) ===\n";
    std::cout << std::left << std::setw(40) << "操作" 
              << std::right << std::setw(12) << "时间(ms)"
              << std::setw(15) << "迭代次数" << "\n"
              << std::string(67, '-') << "\n";

    // std::string append
    {
        std::string base(MEDIUM, 'x');
        std::string append_str("abc");
        auto start = Clock::now();
        for (size_t i = 0; i < 1'000'000; ++i) {
            auto s = base;
            s.append(append_str);
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"std::string::append() (基础: " + std::to_string(MEDIUM) + "B)", 
                      elapsed / 1000.0, 1'000'000});
    }

    // redis::String append
    {
        std::string base_str(MEDIUM, 'x');
        std::string append_str("abc");
        std::pmr::monotonic_buffer_resource mbr;
        String base(base_str, &mbr);
        
        auto start = Clock::now();
        for (size_t i = 0; i < 1'000'000; ++i) {
            String s = base;
            s.append(append_str);
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        print_result({"redis::String::append() (基础: " + std::to_string(MEDIUM) + "B)", 
                      elapsed / 1000.0, 1'000'000});
    }
}

// ==================== Benchmark 4: Size/Capacity 操作 ====================
void benchmark_size_capacity() {
    std::cout << "\n=== Benchmark 4: Size/Capacity 操作 (100M iterations) ===\n";
    std::cout << std::left << std::setw(40) << "操作" 
              << std::right << std::setw(12) << "时间(ms)"
              << std::setw(15) << "迭代次数" << "\n"
              << std::string(67, '-') << "\n";

    // std::string - 获取 size
    {
        std::string test_str(MEDIUM, 'x');
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += test_str.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        print_result({"std::string::size()", elapsed / 1000.0, 100'000'000});
    }

    // redis::String - 获取 size
    {
        std::string test_str(MEDIUM, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        String s(test_str, &mbr);
        
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        print_result({"redis::String::size()", elapsed / 1000.0, 100'000'000});
    }

    // std::string - 获取 capacity
    {
        std::string test_str(MEDIUM, 'x');
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += test_str.capacity();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        print_result({"std::string::capacity()", elapsed / 1000.0, 100'000'000});
    }

    // redis::String - 获取 capacity
    {
        std::string test_str(MEDIUM, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        String s(test_str, &mbr);
        
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s.capacity();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        print_result({"redis::String::capacity()", elapsed / 1000.0, 100'000'000});
    }
}

// ==================== Benchmark 5: 内存开销 ====================
void benchmark_memory_overhead() {
    std::cout << "\n=== Benchmark 5: 内存开销分析 ===\n";
    std::cout << std::left << std::setw(40) << "操作" 
              << std::right << std::setw(20) << "大小(bytes)"
              << std::setw(15) << "开销(bytes)" << "\n"
              << std::string(75, '-') << "\n";

    // std::string 内存
    {
        std::string s1(MEDIUM, 'x');
        std::cout << std::left << std::setw(40) << "std::string (内容: " + std::to_string(MEDIUM) + "B)"
                  << std::right << std::setw(20) << s1.capacity()
                  << std::setw(15) << (s1.capacity() > MEDIUM ? s1.capacity() - MEDIUM : 0) << "\n";
    }

    // redis::String 内存
    {
        std::string test_str(MEDIUM, 'x');
        std::pmr::monotonic_buffer_resource mbr;
        String s(test_str, &mbr);
        size_t capacity = s.capacity();
        size_t overhead = capacity > MEDIUM ? capacity - MEDIUM : 0;
        
        std::cout << std::left << std::setw(40) << "redis::String (内容: " + std::to_string(MEDIUM) + "B)"
                  << std::right << std::setw(20) << capacity
                  << std::setw(15) << overhead << "\n";
    }

    // 比较相对开销
    std::cout << "\n相对开销对比:\n";
    std::string s_str(MEDIUM, 'x');
    std::pmr::monotonic_buffer_resource mbr;
    String s_redis(s_str, &mbr);
    
    double string_overhead = static_cast<double>(std::max(1UL, s_str.capacity() - MEDIUM)) / MEDIUM * 100;
    double redis_overhead = (s_redis.capacity() > MEDIUM ? 
                            static_cast<double>(s_redis.capacity() - MEDIUM) / MEDIUM * 100 : 0.0);
    
    std::cout << std::left << std::setw(40) << "std::string 开销百分比"
              << std::right << std::setw(10) << std::fixed << std::setprecision(2) 
              << string_overhead << "%\n";
    std::cout << std::left << std::setw(40) << "redis::String 开销百分比"
              << std::right << std::setw(10) << std::fixed << std::setprecision(2) 
              << redis_overhead << "%\n";
}

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n"
              << "║    redis::String vs std::string 性能基准测试            ║\n"
              << "║         (编译优化: -O3, CXXFLAGS: -DNDEBUG)              ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n";

    benchmark_string_creation();
    benchmark_copy();
    benchmark_append();
    benchmark_size_capacity();
    benchmark_memory_overhead();

    std::cout << "\n" << std::string(67, '=') << "\n";
    std::cout << "注: 结果可能因系统、编译器和优化级别而异\n";
    std::cout << "建议在相同条件下多次运行以取得平均值\n\n";

    return 0;
}
