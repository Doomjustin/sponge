#ifndef SPONGE_LRU_CACHE_H
#define SPONGE_LRU_CACHE_H

#include <cassert>
#include <list>
#include <memory_resource>
#include <unordered_map>

#include <gsl/gsl>

namespace spg {

template<typename Key, typename Value>
class LRUCache {
    using List = std::pmr::list<std::pair<Key, Value>>;
    using ListIterator = List::iterator;
    using Map = std::pmr::unordered_map<Key, ListIterator>;

public:
    using MemoryResource = std::pmr::memory_resource;

    explicit LRUCache(const size_t capacity, gsl::not_null<MemoryResource*> memory_resource = std::pmr::get_default_resource())
      : capacity_{ capacity },
        items_{ memory_resource.get() },
        cache_{ memory_resource.get() }
    {
        assert(capacity_ > 0);
    }

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    LRUCache(LRUCache&&) = default;
    LRUCache& operator=(LRUCache&&) = default;

    ~LRUCache() = default;

    [[nodiscard]]
    auto get(const Key& key) noexcept -> Value*
    {
        auto it = cache_.find(key);
        if (it == cache_.end())
            return nullptr;

        items_.splice(items_.begin(), items_, it->second);
        return &(it->second->second);
    }

    void put(Key key, Value value)
    {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        if (cache_.size() == capacity_) {
            cache_.erase(items_.back().first);
            items_.pop_back();
        }

        items_.emplace_front(key, std::move(value));
        cache_.emplace(std::move(key), items_.begin());
    }

private:
    size_t capacity_;
    List items_;
    Map cache_;
};

} // namespace spg

#endif // SPONGE_LRU_CACHE_H
