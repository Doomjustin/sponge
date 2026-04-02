#ifndef SPONGE_LEVELDB_SKIP_LIST_H
#define SPONGE_LEVELDB_SKIP_LIST_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <memory_resource>
#include <span>
#include <utility>

#include <gsl/gsl>

#include <sponge/random.h>
#include <sponge/tag.h>

namespace spg::leveldb {

template <typename T, typename Comparator>
class SkipList {
    class Node;

    class alignas(std::atomic<Node*>) Node {
        public:
        const T value;
        int height;

        Node(T value, const int height)
          : value{ std::move(value) },
            height{ height }
        {
            auto* address = forwards();
            for (int i = 0; i < height; ++i)
                new (&address[i]) std::atomic<Node*>{ nullptr };
        }

        auto next(int level) const noexcept -> Node*
        {
            assert(level >= 0 && level < height);
            return forwards()[level].load(std::memory_order_acquire);
        }

        void set_next(int level, Node* x) noexcept
        {
            assert(level >= 0 && level < height);
            forwards()[level].store(x, std::memory_order_release);
        }

        auto set_next_cas(int level, Node* expected, Node* x) noexcept -> bool
        {
            assert(level >= 0 && level < height);
            return forwards()[level].compare_exchange_strong(
                expected, x,
                std::memory_order_release,
                std::memory_order_relaxed
            );
        }

        private:
        auto forwards() noexcept -> std::atomic<Node*>*
        {
            return reinterpret_cast<std::atomic<Node*>*>(this + 1);
        }

        auto forwards() const noexcept -> const std::atomic<Node*>*
        {
            return reinterpret_cast<const std::atomic<Node*>*>(this + 1);
        }
    };

public:
    class Iterator;

    using MemoryResource = std::pmr::memory_resource;

    explicit SkipList(Comparator comparator, gsl::not_null<MemoryResource*> resource = std::pmr::get_default_resource())
      : comparator_{ std::move(comparator) },
        resource_{ resource }
    {
        head_ = allocate_node(T{}, MAX_HEIGHT);
    }

    void insert(const T& value)
    {
        std::array<Node*, MAX_HEIGHT> prev{};
        auto* node = find_greater_or_equal(value, prev);
        assert(!is_equal(value, node));

        const auto new_height = random_height();

        if (auto current_height = max_height_.load(std::memory_order_relaxed);
            new_height > current_height)
        {
            for (int i = current_height; i < new_height; ++i)
                prev[i] = head_;

            while (new_height > current_height)
                if (max_height_.compare_exchange_weak(current_height, new_height, std::memory_order_relaxed))
                    break;
        }

        auto* new_node = allocate_node(value, new_height);
        for (int i = 0; i < new_height; ++i) {
            while (true) {
                auto* next = prev[i]->next(i);
                new_node->set_next(i, next);

                if (prev[i]->set_next_cas(i, next, new_node))
                    break;

                find_greater_or_equal(value, prev);
            }
        }
    }

    [[nodiscard]]
    auto contains(const T& value) const noexcept -> bool
    {
        auto* node = find_greater_or_equal(value, {});
        return is_equal(value, node);
    }

    auto iterator() noexcept -> Iterator
    {
        return Iterator{ *this };
    }

private:
    static constexpr int MAX_HEIGHT = 12;
    static constexpr int BRANCHING = 4;

    Comparator comparator_;
    MemoryResource* resource_;
    std::atomic<int> max_height_ = 1;
    Node* head_;

    [[nodiscard]]
    static auto random_height() noexcept -> int
    {
        constexpr auto p = 1.0 - 1.0 / BRANCHING;
        return std::min(MAX_HEIGHT, random::geometric_failure(p) + 1);
    }

    auto is_after_than(const T& value, const Node* node) const noexcept -> bool
    {
        return node && comparator_(node->value, value) < 0;
    }

    auto is_equal(const T& value, const Node* node) const noexcept -> bool
    {
        return node && comparator_(node->value, value) == 0;
    }

    auto allocate_node(const T& value, int height) -> Node*
    {
        const auto size = sizeof(Node) + height * sizeof(std::atomic<Node*>);
        const auto align = alignof(Node);

        auto* memory = resource_->allocate(size, align);
        return new (memory) Node{ value, height };
    }

    auto find_greater_or_equal(const T& value, std::span<Node*> prev) const noexcept -> Node*
    {
        assert(prev.empty() || prev.size() == MAX_HEIGHT);

        auto* x = head_;
        auto level = max_height_.load(std::memory_order_relaxed) - 1;
        while (true) {
            auto* next = x->next(level);
            if (is_after_than(value, next)) {
                x = next;
            }
            else {
                if (!prev.empty())
                    prev[level] = x;

                if (level == 0)
                    return next;

                --level;
            }
        }

        std::unreachable();
    }

    auto find_less_than(const T& value) const noexcept -> Node*
    {
        auto* x = head_;
        auto level = max_height_.load(std::memory_order_relaxed) - 1;

        while (true) {
            auto* next = x->next(level);
            if (!next || comparator_(next->value, value) >= 0) {
                if (level == 0)
                    return x;

                --level;
            }
            else {
                x = next;
            }
        }

        std::unreachable();
    }

    auto find_last() const noexcept -> Node*
    {
        auto* x = head_;
        auto level = max_height_.load(std::memory_order_relaxed) - 1;

        while (true) {
            auto* next = x->next(level);
            if (!next) {
                if (level == 0)
                    return x;
                --level;
            }
            else {
                x = next;
            }
        }

        std::unreachable();
    }
};


template<typename T, typename C>
class SkipList<T, C>::Iterator {
public:
    explicit Iterator(SkipList<T, C>& skip_list)
      : skip_list_{ skip_list }
    {}

    [[nodiscard]]
    auto is_valid() const noexcept -> bool
    {
        return current_;
    }

    [[nodiscard]]
    auto value() const noexcept -> T
    {
        assert(is_valid());
        return current_->value;
    }

    void next() noexcept
    {
        assert(is_valid());
        current_ = current_->next(0);
    }

    void prev() noexcept
    {
        assert(is_valid());
        current_ = skip_list_.find_less_than(value());
        if (current_ == skip_list_.head_)
            current_ = nullptr;
    }

    void seek(const T& value) noexcept
    {
        current_ = skip_list_.find_greater_or_equal(value, {});
    }

    void seek([[maybe_unused]] ToFirstT) noexcept
    {
        current_ = skip_list_.head_->next(0);
    }

    void seek([[maybe_unused]] ToLastT) noexcept
    {
        current_ = skip_list_.find_last();
        if (current_ == skip_list_.head_)
            current_ = nullptr;
    }

private:
    SkipList<T, C>& skip_list_;
    Node* current_ = nullptr;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_SKIP_LIST_H
