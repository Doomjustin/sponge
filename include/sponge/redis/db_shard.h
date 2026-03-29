#ifndef SPONGE_REDIS_DB_SHARD_H
#define SPONGE_REDIS_DB_SHARD_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

#include <sponge/hash.h>
#include <sponge/tag.h>

#include "dash_table.h"
#include "sorted_set.h"
#include "ttl_manager.h"

namespace spg::redis {

// 支持的类型
// string
// int
// hash
// list
// 需要先对key进行hash，定位到具体的shard，再在shard内部进行操作
class DBShard {
public:
    using String = std::pmr::string;
    using Integral = std::int64_t;
    using ZSet = SortedSet;
    // using HashTable = std::pmr::unordered_map<String, String>;
    // using Flathash = std::pmr::vector<std::pair<String, String>>;
    // using List = std::pmr::list<String>;
    using Key = String;
    using Value = std::variant<String, Integral, ZSet>;
    using Size = size_t;
    using MemoryResource = std::pmr::memory_resource;
    using MilliSeconds = TTLManager::Milliseconds;

    struct Entry {
        Value value;
        std::int64_t expire_at;
    };
    
    using ReadRange = DashTable<Entry>::ReadRange;
    using ModifyRange = DashTable<Entry>::ModifyRange;
    using LockedReadView = DashTable<Entry>::LockedReadView;
    using LockedModifyView = DashTable<Entry>::LockedModifyView;

    class EntryHandler;

    // 修改路径，内部是unique_lock，保证独占访问
    // 由于每次访问都需要判断是否过期，所以即使是只读操作也要使用modify接口，保证过期逻辑的正确性
    template<typename Func>
    auto modify(std::string_view key, Func&& func) -> decltype(auto);

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return table_.size();
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        return table_.empty();
    }

    auto mutex_of(std::string_view key) -> std::shared_mutex&
    {
        return table_.mutex_of(key);
    }

    auto get_entry(std::string_view key) -> Entry*
    {
        return table_.get(key, unlock);
    }

    void set(std::string_view key, Entry value)
    {
        table_.set(key, std::move(value), unlock);
    }

    auto erase(std::string_view key) -> bool
    {
        return table_.erase(key, unlock);
    }

    auto range(ReadOnlyTag read_only) const noexcept -> ReadRange
    {
        return table_.range(read_only);
    }

    auto range(ReadWriteTag read_write) noexcept -> ModifyRange
    {
        return table_.range(read_write);
    }

    void clear()
    {
        table_.clear();
    }

private:
    DashTable<Entry> table_;
    TTLManager ttl_manager_;
};


class DBShard::EntryHandler {
public:
    EntryHandler(DashTable<Entry>::Segment &segment, DashTable<Entry>::Segment::iterator it, std::string_view key)
      : segment_{ segment }, 
        it_{ it }, 
        key_{ key }
    {}

    [[nodiscard]]
    auto exists() const noexcept -> bool
    {
        return it_ != segment_.end();
    }
    
    template<typename T>
    auto get_if(std::type_identity<T>) -> T*
    {
        if (!exists()) return nullptr;
        return std::get_if<T>(&it_->second.value);
    }

    template<typename T>
    auto get_if(std::type_identity<T>) const -> const T*
    {
        if (!exists()) return nullptr;
        return std::get_if<T>(&it_->second.value);
    }

    template<typename T, typename... Args>
    auto emplace(std::type_identity<T>, Args&&... args) -> T*
    {
        Entry entry {
            .value = T{ std::forward<Args>(args)... },
            .expire_at = TTLManager::PERSISTENT
        };

        auto [iter, emplaced] = segment_.emplace(key_, std::move(entry));
        it_ = iter;
        return std::get_if<T>(&it_->second.value);
    }

    auto expire(int64_t ms) -> bool
    {
        if (!exists()) return false;

        it_->second.expire_at = TTLManager::expire_at(ms);
        return true;
    }

    auto erase() -> bool
    {
        if (!exists()) return false;

        segment_.erase(it_);
        it_ = segment_.end();
        return true;
    }

    auto persist() -> bool
    {
        if (!exists()) return false;

        it_->second.expire_at = TTLManager::PERSISTENT;
        return true;
    }
    
    auto ttl() -> std::optional<int64_t>
    {
        if (!exists()) return {};

        auto expire_at = it_->second.expire_at;
        if (TTLManager::is_persist(expire_at))
            return TTLManager::PERSISTENT;

        if (TTLManager::is_expired(expire_at)) {
            segment_.erase(it_);
            it_ = segment_.end();
            return {};
        }

        return TTLManager::ttl(expire_at);
    }

    auto rename(std::string_view new_key) -> bool
    {
        if (!exists()) return false;

        if (new_key == key_)
            return true;

        auto entry = std::move(it_->second);
        segment_.erase(it_);
        it_ = segment_.end();

        auto [iter, _] = segment_.insert_or_assign(String{ new_key }, std::move(entry));
        it_ = iter;
        return true;
    }

private:
    DashTable<Entry>::Segment& segment_;
    DashTable<Entry>::Segment::iterator it_;
    std::string_view key_;
};


template<typename Func>
auto DBShard::modify(std::string_view key, Func&& func) -> decltype(auto)
{
    return table_.modify(key, [&] (auto& segment, auto it) {
        if (it != segment.end() && ttl_manager_.is_expired(it->second.expire_at)) {
            segment.erase(it);
            it = segment.end();
        }

        // 由于是模板，所以必须要保证能看到handler的完整定义
        EntryHandler handler{ segment, it, key };
        return func(handler);
    });
}

} // namespace spg::redis

#endif // SPONGE_REDIS_DB_SHARD_H
