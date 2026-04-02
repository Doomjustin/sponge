#ifndef SPONGE_REDIS_SKIP_LIST_H
#define SPONGE_REDIS_SKIP_LIST_H

#include <array>
#include <cassert>
#include <cstddef>
#include <memory_resource>
#include <string_view>
#include <utility>

#include <sponge/range.h>
#include <sponge/tag.h>

namespace spg::redis {

// 这是 Redis 风格的内部跳表组件，不是通用容器。
// 设计约束：仅供上层 dict 协同调用；关键前置条件由上层保证。
// 因此部分接口在前置条件不满足时使用 assert 失败，而不是返回错误码。
class SkipList {
public:
    // 顺序迭代器，按 (score, element) 升序遍历。
    class Iterator;
    // 逆序迭代器，按 (score, element) 降序遍历。
    class ReverseIterator;

    // 使用默认内存资源构造跳表。
    SkipList();

    // 使用指定内存资源构造跳表。
    explicit SkipList(std::pmr::memory_resource* resource);
    
    SkipList(const SkipList&) = delete;
    auto operator=(const SkipList&) -> SkipList& = delete;

    SkipList(SkipList&& other) noexcept;
    auto operator=(SkipList&& other) noexcept -> SkipList&;

    ~SkipList();

    // 插入一个元素。
    // 允许重复插入相同 (score, element)。
    // 使用 string_view 保存 element，调用方需保证其生命周期足够长。
    void insert(double score, std::string_view element);

    // 删除一个匹配的 (score, element)，找到则删除并返回 true。
    // 若存在重复项，只删除其中一个。
    auto erase(double score, std::string_view element) -> bool;

    // 更新元素分数：将 (old_score, element) 改为 (new_score, element)。
    // 前置条件：目标元素必须存在（由上层 dict 保证）。
    // 若前置条件不成立，Debug 构建下会触发 assert。
    // 更新后元素位置可能改变。
    void update(double old_score, double new_score, std::string_view element);

    // 元素数量。
    [[nodiscard]]
    constexpr auto size() const noexcept -> size_t
    {
        return length_;
    }

    // 是否包含给定 (score, element)。
    [[nodiscard]]
    auto contains(double score, std::string_view element) const -> bool;

    // 返回给定元素的 rank（从 0 开始）。
    // 语义：rank 等于“严格小于该元素的节点个数”。
    // 当元素不存在时返回 0。
    [[nodiscard]]
    auto rank(double score, std::string_view element) const -> size_t;

    // 返回 score 落在 [min_score, max_score] 区间内的元素数量。
    // 仅按 score 比较，不考虑 element 维度。
    [[nodiscard]]
    auto count_by_score(double min_score, double max_score) const -> size_t;

    // 查找给定元素，找不到则返回 end()。
    [[nodiscard]]
    auto find(double score, std::string_view element) const -> Iterator;

    // 按 rank 查找元素，rank 从 1 开始。
    [[nodiscard]]
    auto find(size_t rank, ByRankTag by_rank) const -> Iterator;

    // 判断给定分数区间是否与当前跳表的 score 投影区间相交。
    // 只基于最小/最大 score 判定，不关心 element 维度。
    template<Boundary Left, Boundary Right>
    auto overlaps(const Range<double, Left, Right>& range) const -> bool
    {
        if (length_ == 0)
            return false;

        auto [min_score, max_score] = score_range();
        Range<double, Boundary::Inclusive, Boundary::Inclusive> query_range{ min_score, max_score };
        return range.overlaps(query_range);
    }

    // 升序迭代起点与终点。
    auto begin() -> Iterator;
    auto end() -> Iterator;
    auto begin() const -> Iterator;
    auto end() const -> Iterator;

    // 降序迭代起点与终点。
    auto rbegin() -> ReverseIterator;
    auto rend() -> ReverseIterator;
    auto rbegin() const -> ReverseIterator;
    auto rend() const -> ReverseIterator;

private:
    struct Node;
    struct Level;

    static constexpr size_t MAX_LEVEL = 64;
    static constexpr double P = 0.25;

    std::pmr::memory_resource* resource_;
    Node* head_;
    Node* tail_ = nullptr;
    size_t level_ = 1;
    size_t length_ = 0;

    static auto random_level() -> size_t;

    static auto is_equal(const Node* node, double score, std::string_view element) -> bool;
    
    static auto is_less_than(const Node* node, double score, std::string_view element) -> bool;

    static auto is_less_equal_than(const Node* node, double score, std::string_view element) -> bool;

    auto create_node(size_t level, double score, std::string_view element) -> Node*;

    void destroy_node(Node* node) noexcept;

    void clear_nodes() noexcept;

    void reset_empty();

    void erase_node(Node* node, std::array<Node*, MAX_LEVEL>& update);

    auto score_range() const noexcept -> std::pair<double, double>;
};


class SkipList::Iterator {
public:
    // 使用内部节点构造迭代器。
    explicit Iterator(Node* node);

    // 读取当前元素。
    auto operator*() const -> std::pair<double, std::string_view>;

    // 前进到下一个元素。
    auto operator++() -> Iterator&;
    auto operator++(int) -> Iterator;

    // 退回到上一个元素。
    auto operator--() -> Iterator&;
    auto operator--(int) -> Iterator;

    // 比较两个迭代器是否指向同一位置。
    auto operator==(const Iterator& other) const -> bool;
    auto operator!=(const Iterator& other) const -> bool;

private:
    Node* current_;
};


class SkipList::ReverseIterator {
public:
    // 使用内部节点构造逆序迭代器。
    explicit ReverseIterator(Node* node);

    // 读取当前元素。
    auto operator*() const -> std::pair<double, std::string_view>;

    // 逆序前进（指向更小的元素）。
    auto operator++() -> ReverseIterator&;
    auto operator++(int) -> ReverseIterator;

    // 逆序后退（指向更大的元素）。
    auto operator--() -> ReverseIterator&;
    auto operator--(int) -> ReverseIterator;

    // 比较两个迭代器是否指向同一位置。
    auto operator==(const ReverseIterator& other) const -> bool;
    auto operator!=(const ReverseIterator& other) const -> bool;

private:
    Node* current_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_SKIP_LIST_H
