#ifndef SPONGE_REDIS_STRING_H
#define SPONGE_REDIS_STRING_H

#include <array>
#include <concepts>
#include <iterator>
#include <memory_resource>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <gsl/gsl>

namespace spg::redis {

// 更激进扩容策略的标签类型。
struct ExpandGreedyTag{};
static constexpr ExpandGreedyTag greedy{};

// 基于 SDS 的 Redis 风格动态字符串。
//
// 缓冲区始终以 '\0' 结尾，size() 记录逻辑长度。
// 内存由传入的 polymorphic memory resource 提供。
class String {
public:
    // 使用默认内存资源 默认构造，创建一个空字符串。
    // std::pmr::get_default_resource()。
    String();

    // 使用默认内存资源 用 str 初始化字符串。
    // std::pmr::get_default_resource()。
    explicit String(std::string_view str);

    // 使用指定的内存资源创建空字符串。
    explicit String(gsl::not_null<std::pmr::memory_resource*> resource);

    // 使用指定的内存资源用 str 初始化字符串。
    String(std::string_view str, gsl::not_null<std::pmr::memory_resource*> resource);

    // 深拷贝构造。
    String(const String& other);

    // 深拷贝赋值，会先释放当前存储。
    auto operator=(const String& other) -> String&;

    // 移动构造，转移内部 SDS 缓冲区所有权。
    String(String&& other) noexcept;

    // 移动赋值，先释放当前存储，再接管对方缓冲区。
    // 如果无法确定两者是否使用同一内存资源，则退化为深拷贝。
    // 所以这里不能标记为 noexcept。
    auto operator=(String&& other) -> String&;

    ~String();

    // 在末尾追加 str。
    // 当 available() 小于 str.size() 时可能触发重分配。
    void append(std::string_view str);

    // 保证容量至少为 new_size。
    // 不改变当前内容。
    void reserve(size_t new_size);

    // 使用 greedy 策略保证容量至少为 new_size。
    // 小于 MAX_PREALLOC 时优先几何扩容。
    void reserve(size_t new_size, ExpandGreedyTag greedy);

    // 将长度扩展到 new_size，并把新增区域填充为 0。
    // 若 new_size <= size()，则不执行扩容路径。
    void resize(size_t new_size);

    // 将逻辑长度清零，保留已分配容量。
    void clear() noexcept;

    // 将容量收缩到当前 size()，释放多余内存。
    void shrink_to_fit();

    // 用 str 替换当前内容。
    // 当容量足够时复用现有存储。
    void assign(std::string_view str);

    // 当前逻辑长度（字节数）。
    [[nodiscard]]
    auto size() const noexcept -> size_t;

    // 可存放有效数据的总容量（不含结尾 '\0'）。
    [[nodiscard]]
    auto capacity() const noexcept -> size_t;

    // 在需要重分配前，剩余可写字节数。
    [[nodiscard]]
    auto available() const noexcept -> size_t;

    // 是否为空字符串。
    [[nodiscard]]
    auto empty() const noexcept -> bool
    {
        return size() == 0;
    }

    // 当前字符串数据的非拥有视图。
    [[nodiscard]]
    auto view() const noexcept -> std::string_view;

    // 不做边界检查的元素访问。
    auto operator[](size_t index) noexcept -> char&
    {
        return sds_[index];
    }

    // 不做边界检查的只读元素访问。
    auto operator[](size_t index) const noexcept -> char
    {
        return sds_[index];
    }

    // 指向首字符的指针。
    auto begin() noexcept -> char* { return sds_; }

    // 指向逻辑末尾后一位的指针。
    auto end() noexcept -> char* { return sds_ + size(); }

    [[nodiscard]]
    // 指向首字符的只读指针。
    auto begin() const noexcept -> const char* { return sds_; }

    [[nodiscard]]
    // 指向逻辑末尾后一位的只读指针。
    auto end() const noexcept -> const char* { return sds_ + size(); }

private:
    static constexpr size_t EXPAND_FACTOR = 2;
    static constexpr size_t MAX_PREALLOC{ 1024 * 1024 };

    std::pmr::memory_resource* resource_;
    char* sds_;
};

// 将整数转换为字符串，使用指定的内存资源分配结果。
template<std::integral T, bool FollowOriginal = false>
auto string_cast(T value, gsl::not_null<std::pmr::memory_resource*> resource) -> String
{
    // 和原作一样的实现，只作为性能对比基准。基本用不上
    if constexpr (FollowOriginal) {
        using Unsigned = std::make_unsigned_t<T>;

        Unsigned magnitude{};
        if constexpr (std::is_signed_v<T>) {
            if (value < 0) {
                // 先在有符号范围内计算 -(value + 1)，再补 1，避免最小值取负溢出。
                magnitude = static_cast<Unsigned>(-(value + 1));
                ++magnitude;
            }
            else {
                magnitude = static_cast<Unsigned>(value);
            }
        }
        else {
            magnitude = value;
        }

        std::array<char, 21> buffer{};
        auto* const begin = buffer.data();
        auto* ptr = begin;

        do {
            *ptr++ = static_cast<char>('0' + (magnitude % 10));
            magnitude /= 10;
        } while (magnitude > 0);

        if constexpr (std::is_signed_v<T>)
            if (value < 0) *ptr++ = '-';

        const auto size = ptr - begin;
        *ptr = '\0';

        auto* left = begin;
        auto* right = ptr - 1;
        while (left < right) {
            std::swap(*left, *right);
            ++left;
            --right;
        }

        return { std::string_view(begin, static_cast<size_t>(size)), resource };
    }

    fmt::memory_buffer buffer{};
    fmt::format_to(std::back_inserter(buffer), "{}", value);
    return { std::string_view(buffer.data(), buffer.size()), resource };
}

// 将整数转换为字符串，使用默认内存资源分配结果。
template<std::integral T>
auto string_cast(T value) -> String
{
    return string_cast<T, false>(value, std::pmr::get_default_resource());
}

// 为 String 提供 fmt 格式化支持。
auto format_as(const String& str) -> std::string_view;

// 使用 fmt 格式化字符串，结果存储在 String 中，使用指定的内存资源分配。
template<typename... T>
auto format(gsl::not_null<std::pmr::memory_resource*> resource, fmt::format_string<T...> fmt_str, T&&... args) -> String
{
    fmt::memory_buffer buffer{};
    fmt::format_to(std::back_inserter(buffer), fmt_str, std::forward<T>(args)...);
    return { std::string_view(buffer.data(), buffer.size()), resource };
}

// 使用 fmt 格式化字符串，结果存储在 String 中，使用默认内存资源分配。
template<typename... T>
auto format(fmt::format_string<T...> fmt_str, T&&... args) -> String
{
    return format(std::pmr::get_default_resource(), fmt_str, std::forward<T>(args)...);
}

} // namespace spg::redis

#endif // SPONGE_REDIS_STRING_H
