#include "skip_list.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>

#include <sponge/random.h>

namespace spg::redis {

struct SkipList::Level {
    Node* forward = nullptr;
    size_t span = 0;
};


struct SkipList::Node {
    double score;
    std::string_view element;
    Node* backward = nullptr;
    size_t level_count = 0;

    [[nodiscard]]
    auto next(size_t lvl) noexcept -> Node*
    {
        return level(lvl)->forward;
    }
    
    [[nodiscard]]
    auto next(size_t lvl) const noexcept -> const Node*
    {
        return level(lvl)->forward;
    }

    [[nodiscard]]
    auto level(size_t lvl = 0) noexcept -> Level*
    {
        return reinterpret_cast<Level*>(this + 1) + lvl;
    }

    [[nodiscard]]
    auto level(size_t lvl = 0) const noexcept -> const Level*
    {
        return reinterpret_cast<const Level*>(this + 1) + lvl;
    }
};


SkipList::Iterator::Iterator(Node* node) 
  : current_{ node } 
{}

auto SkipList::Iterator::operator*() const -> std::pair<double, std::string_view>
{
    assert(current_ != nullptr);
    return { current_->score, current_->element };
}

auto SkipList::Iterator::operator++() -> Iterator&
{
    assert(current_ != nullptr);
    current_ = current_->next(0);
    return *this;
}

auto SkipList::Iterator::operator++(int) -> Iterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

auto SkipList::Iterator::operator--() -> Iterator&
{
    assert(current_ != nullptr);
    current_ = current_->backward;
    return *this;
}

auto SkipList::Iterator::operator--(int) -> Iterator
{
    auto temp = *this;
    --(*this);
    return temp;
}

auto SkipList::Iterator::operator==(const Iterator& other) const -> bool
{
    return current_ == other.current_;
}

auto SkipList::Iterator::operator!=(const Iterator& other) const -> bool
{
    return current_ != other.current_;
}


SkipList::ReverseIterator::ReverseIterator(Node* node)
  : current_{ node }
{}

auto SkipList::ReverseIterator::operator*() const -> std::pair<double, std::string_view>
{
    assert(current_ != nullptr);
    return { current_->score, current_->element };
}

auto SkipList::ReverseIterator::operator++() -> ReverseIterator&
{
    assert(current_ != nullptr);
    current_ = current_->backward;
    return *this;
}

auto SkipList::ReverseIterator::operator++(int) -> ReverseIterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

auto SkipList::ReverseIterator::operator--() -> ReverseIterator&
{
    assert(current_ != nullptr);
    current_ = current_->next(0);
    return *this;
}

auto SkipList::ReverseIterator::operator--(int) -> ReverseIterator
{
    auto temp = *this;
    --(*this);
    return temp;
}

auto SkipList::ReverseIterator::operator==(const ReverseIterator& other) const -> bool
{
    return current_ == other.current_;
}

auto SkipList::ReverseIterator::operator!=(const ReverseIterator& other) const -> bool
{
    return current_ != other.current_;
}


SkipList::SkipList()
  : SkipList{ std::pmr::get_default_resource() }
{}

SkipList::SkipList(std::pmr::memory_resource* resource)
  : resource_{ resource },
    head_{ create_node(MAX_LEVEL, 0.0, "") }
{
}

SkipList::~SkipList()
{
    auto* current = head_;
    Node* next;
    while (current) {
        next = current->next(0);
        destroy_node(current);
        current = next;
    }
}

void SkipList::insert(double score, std::string_view element)
{
    assert(!std::isnan(score));

    std::array<Node*, MAX_LEVEL> update{};
    std::array<size_t, MAX_LEVEL> rank{};

    auto* current = head_;
    for (auto i = level_; i > 0; --i) {
        rank[i - 1] = (i == level_) ? 0 : rank[i];

        while (is_less_than(current->next(i - 1), score, element)) 
        {
            rank[i - 1] += current->level(i - 1)->span;
            current = current->next(i - 1);
        }

        update[i - 1] = current;
    }

    auto new_level = random_level();
    if (new_level > level_) {
        for (auto i = level_; i < new_level; ++i) {
            update[i] = head_;
            rank[i] = 0;
            update[i]->level(i)->span = length_;
        }
        
        level_ = new_level;
    }

    auto new_node = create_node(new_level, score, element);
    for (size_t i = 0; i < new_level; ++i) {
        new_node->level(i)->forward = update[i]->level(i)->forward;
        update[i]->level(i)->forward = new_node;

        new_node->level(i)->span = update[i]->level(i)->span - (rank[0] - rank[i]);
        update[i]->level(i)->span = (rank[0] - rank[i]) + 1;   
    }

    for (size_t i = new_level; i < level_; ++i)
        update[i]->level(i)->span += 1;

    new_node->backward = update[0] == head_ ? nullptr : update[0];

    if (new_node->next(0))
        new_node->next(0)->backward = new_node;
    else
        tail_ = new_node;

    ++length_;
}

auto SkipList::erase(double score, std::string_view element) -> bool
{
    std::array<Node*, MAX_LEVEL> update{};

    auto* current = head_;
    for (auto i = level_; i > 0; --i) {
        while (is_less_than(current->next(i - 1), score, element))
            current = current->next(i - 1);

        update[i - 1] = current;
    }

    current = current->next(0);
    if (!is_equal(current, score, element))
        return false;

    erase_node(current, update);
    return true;
}

void SkipList::update(double old_score, double new_score, std::string_view element)
{
    std::array<Node*, MAX_LEVEL> update{};
    auto* current = head_;
    for (auto i = level_; i > 0; --i) {
        while (is_less_than(current->next(i - 1), old_score, element))
            current = current->next(i - 1);

        update[i - 1] = current;
    }

    current = current->next(0);
    // 不允许更新不存在的元素，所以必须找到这个元素。
    assert(is_equal(current, old_score, element));
    
    // 如果插入后能保持原来的顺序，那么直接更新score就行了，否则需要删除原来的节点再插入一个新节点。
    if ((!current->next(0) || current->next(0)->score > new_score) &&
        (!current->backward || current->backward->score < new_score)) {
        current->score = new_score;
    }
    else {
        erase_node(current, update);
        insert(new_score, element); 
    }
}

auto SkipList::contains(double score, std::string_view element) const -> bool
{
    return find(score, element) != end();
}

auto SkipList::rank(double score, std::string_view element) const -> size_t
{
    size_t rank = 1;
    auto* current = head_;
    for (auto i = level_; i > 0; --i) {
        while (is_less_than(current->next(i - 1), score, element)) {
            rank += current->level(i - 1)->span;
            current = current->next(i - 1);
        }
    }
        
    if (is_equal(current->next(0), score, element))
        return rank;

    return 0;
}

auto SkipList::find(double score, std::string_view element) const -> Iterator
{
    auto* current = head_;
    for (auto i = level_; i > 0; --i) {
        while (is_less_than(current->next(i - 1), score, element))
            current = current->next(i - 1);
    }

    current = current->next(0);
    if (!is_equal(current, score, element))
        return Iterator{ nullptr };

    return Iterator{ current };
}

auto SkipList::find(size_t rank, ByRankTag by_rank) const -> Iterator
{
    // rank is 1-based: rank 1 = first element
    if (rank == 0 || rank > length_)
        return Iterator{ nullptr };

    size_t traversed = 0;
    auto* current = head_;
    
    for (auto i = level_; i > 0; --i) {
        // Advance while next element's span fits within remaining rank
        while (current->next(i - 1) && traversed + current->level(i - 1)->span < rank) {
            traversed += current->level(i - 1)->span;
            current = current->next(i - 1);
        }
    }

    // Move to next on level 0
    current = current->next(0);
    // Now traversed+1 should equal rank
    return (traversed + 1 == rank && current) ? Iterator{ current } : Iterator{ nullptr };
}

auto SkipList::begin() -> Iterator
{
    return Iterator{ head_->next(0) };
}

auto SkipList::end() -> Iterator
{
    return Iterator{ nullptr };
}

auto SkipList::begin() const -> Iterator
{
    return Iterator{ head_->next(0) };
}

auto SkipList::end() const -> Iterator
{
    return Iterator{ nullptr };
}

auto SkipList::rbegin() -> ReverseIterator
{
    return ReverseIterator{ tail_ };
}

auto SkipList::rend() -> ReverseIterator
{
    return ReverseIterator{ nullptr };
}

auto SkipList::rbegin() const -> ReverseIterator
{
    return ReverseIterator{ tail_ };
}

auto SkipList::rend() const -> ReverseIterator
{
    return ReverseIterator{ nullptr };
}

auto SkipList::random_level() -> size_t
{
    // 原作的实现过于原始，懒得抄了，直接用一个几何分布生成器，效率更高，质量更好。
    static_assert(0.0 < P && P < 1.0, "P must be in (0, 1)");

    // 有P的概率增加一层，直到达到MAX_LEVEL。
    // 这里的参数是不增加的概率，所以是1-P。
    return std::min(random::geometric_failure<size_t>(1 - P) + 1, MAX_LEVEL);
}

auto SkipList::is_equal(const Node* node, double score, std::string_view element) -> bool
{
    return node && node->score == score && node->element == element;
}

auto SkipList::is_less_than(const Node* node, double score, std::string_view element) -> bool
{
    return node && (node->score < score ||
                   (node->score == score && node->element < element));
}

auto SkipList::is_less_equal_than(const Node* node, double score, std::string_view element) -> bool
{
    return node && (node->score < score ||
                   (node->score == score && node->element <= element));
}

auto SkipList::create_node(size_t level, double score, std::string_view element) -> Node*
{
    auto total_size = sizeof(Node) + level * sizeof(Level);
    auto* node = static_cast<Node*>(resource_->allocate(total_size, alignof(Node)));

    std::construct_at(node, score, element, nullptr, level);
    
    auto* forwards = node->level();
    std::uninitialized_value_construct(forwards, forwards + level);

    return node;
}

void SkipList::destroy_node(Node* node) noexcept
{
    if (node == nullptr)
        return;

    auto forwards = node->level();
    auto level = node->level_count;
    std::destroy(forwards, forwards + level);

    std::destroy_at(node);
    auto total_size = sizeof(Node) + level * sizeof(Level);
    resource_->deallocate(node, total_size, alignof(Node));
}

void SkipList::erase_node(Node* node, std::array<Node*, MAX_LEVEL>& update)
{
    for (size_t i = 0; i < level_; ++i) {
        if (update[i]->level(i)->forward == node) {
            update[i]->level(i)->span += node->level(i)->span - 1;
            update[i]->level(i)->forward = node->level(i)->forward;
        }
        else {
            update[i]->level(i)->span -= 1;
        }
    }

    if (node->next(0))
        node->next(0)->backward = node->backward;
    else
        tail_ = node->backward;

    while (level_ > 1 && head_->level(level_ - 1)->forward == nullptr)
        --level_;

    destroy_node(node);
    --length_;
}

auto SkipList::score_range() const noexcept -> std::pair<double, double>
{
    auto min_score = head_->next(0) ? head_->next(0)->score : 0.0;
    auto max_score = tail_ ? tail_->score : 0.0;
    return { min_score, max_score };
}

} // namespace spg::redis
