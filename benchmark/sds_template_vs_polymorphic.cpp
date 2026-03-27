#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/gsl>

#include <sponge/redis/string.h>

// 需要访问 SDS 的内部实现
#include "../src/redis/sds.h"

using namespace spg::redis;
using Clock = std::chrono::high_resolution_clock;
using Microseconds = std::chrono::microseconds;

// ==================== 模板版本的 SDS String ====================
// 这个版本直接用模板参数指定资源类型，避免虚函数

template<typename MemoryResource>
class TemplateString {
public:
    explicit TemplateString(gsl::not_null<MemoryResource*> resource)
        : resource_{ resource }, sds_{ nullptr }
    {}

    explicit TemplateString(std::string_view str, gsl::not_null<MemoryResource*> resource)
        : resource_{ resource }
    {
        SDS manager{ resource_ };
        sds_ = manager.create(str);
    }

    ~TemplateString() {
        if (sds_ != nullptr) {
            SDS manager{ resource_ };
            manager.destroy(sds_);
            sds_ = nullptr;
        }
    }

    [[nodiscard]]
    auto size() const noexcept -> size_t {
        return SDS::size(sds_);
    }

    [[nodiscard]]
    auto capacity() const noexcept -> size_t {
        return SDS::capacity(sds_);
    }

    [[nodiscard]]
    auto available() const noexcept -> size_t {
        return SDS::available(sds_);
    }

private:
    MemoryResource* resource_;
    char* sds_;
};

// ==================== Benchmark 1: size() 调用对比 ====================
void benchmark_size_call() {
    std::cout << "\n=== Benchmark: size() 调用 (500M iterations) ===\n";
    std::cout << std::left << std::setw(50) << "实现类型" 
              << std::right << std::setw(15) << "时间(ms)"
              << std::setw(20) << "迭代次数" << "\n"
              << std::string(85, '-') << "\n";

    std::string test_str(100, 'x');

    // 多态版本 - PMR
    {
        std::pmr::monotonic_buffer_resource mbr;
        String s(test_str, &mbr);
        
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 500'000'000; ++i) {
            sum += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        std::cout << std::left << std::setw(50) << "多态 String (PMR 虚函数)"
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << elapsed / 1000.0 << " ms"
                  << std::setw(20) << 500'000'000 << "\n";
    }

    // 模板版本 - 具体类型
    {
        std::pmr::monotonic_buffer_resource mbr;
        TemplateString<std::pmr::monotonic_buffer_resource> s(test_str, &mbr);
        
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 500'000'000; ++i) {
            sum += s.size();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        std::cout << std::left << std::setw(50) << "模板 TemplateString<monotonic_buffer_resource>"
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << elapsed / 1000.0 << " ms"
                  << std::setw(20) << 500'000'000 << "\n";
    }
}

// ==================== Benchmark 2: capacity() 调用对比 ====================
void benchmark_capacity_call() {
    std::cout << "\n=== Benchmark: capacity() 调用 (100M iterations) ===\n";
    std::cout << std::left << std::setw(50) << "实现类型" 
              << std::right << std::setw(15) << "时间(ms)"
              << std::setw(20) << "迭代次数" << "\n"
              << std::string(85, '-') << "\n";

    std::string test_str(100, 'x');

    // 多态版本
    {
        std::pmr::monotonic_buffer_resource mbr;
        String s(test_str, &mbr);
        
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s.capacity();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        std::cout << std::left << std::setw(50) << "多态 String (PMR 虚函数)"
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << elapsed / 1000.0 << " ms"
                  << std::setw(20) << 100'000'000 << "\n";
    }

    // 模板版本
    {
        std::pmr::monotonic_buffer_resource mbr;
        TemplateString<std::pmr::monotonic_buffer_resource> s(test_str, &mbr);
        
        auto start = Clock::now();
        volatile size_t sum = 0;
        for (size_t i = 0; i < 100'000'000; ++i) {
            sum += s.capacity();
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        (void)sum;
        std::cout << std::left << std::setw(50) << "模板 TemplateString<monotonic_buffer_resource>"
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << elapsed / 1000.0 << " ms"
                  << std::setw(20) << 100'000'000 << "\n";
    }
}

// ==================== Benchmark 3: 创建操作对比 ====================
void benchmark_creation() {
    std::cout << "\n=== Benchmark: 创建字符串 (5M iterations) ===\n";
    std::cout << std::left << std::setw(50) << "实现类型" 
              << std::right << std::setw(15) << "时间(ms)"
              << std::setw(20) << "迭代次数" << "\n"
              << std::string(85, '-') << "\n";

    std::string test_str(100, 'x');

    // 多态版本
    {
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            volatile auto s = String(test_str, &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        std::cout << std::left << std::setw(50) << "多态 String (PMR 虚函数)"
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << elapsed / 1000.0 << " ms"
                  << std::setw(20) << 5'000'000 << "\n";
    }

    // 模板版本
    {
        std::pmr::monotonic_buffer_resource mbr;
        auto start = Clock::now();
        for (size_t i = 0; i < 5'000'000; ++i) {
            volatile auto s = TemplateString<std::pmr::monotonic_buffer_resource>(test_str, &mbr);
            (void)s;
        }
        auto elapsed = std::chrono::duration_cast<Microseconds>(Clock::now() - start).count();
        std::cout << std::left << std::setw(50) << "模板 TemplateString<monotonic_buffer_resource>"
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << elapsed / 1000.0 << " ms"
                  << std::setw(20) << 5'000'000 << "\n";
    }
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n"
              << "║  多态 String vs 模板 String 性能对比                      ║\n"
              << "║  测试虚函数调用的真实性能影响                             ║\n"
              << "║  (编译优化: -O3, CXXFLAGS: -DNDEBUG)                      ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n";

    benchmark_size_call();
    benchmark_capacity_call();
    benchmark_creation();

    std::cout << "\n" << std::string(85, '=') << "\n";
    std::cout << "分析:\n";
    std::cout << "- 若模板版本快很多: 虚函数调用是真正的瓶颈\n";
    std::cout << "- 若差别不大: 问题在其他地方（如 PMR 分配策略）\n";
    std::cout << "- 若模板版本快但有限: 编译器优化已经接近极限\n\n";

    return 0;
}
