#ifndef SPONGE_REDIS_DB_SHARD_H
#define SPONGE_REDIS_DB_SHARD_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

#include <sponge/hash.h>

#include "dash_table.h"
#include "sorted_set.h"
#include "ttl_manager.h"

namespace spg::redis {

template<typename T>
inline constexpr std::type_identity<T> as_type{};


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

    class EntryHandler {
    public:
        EntryHandler(DashTable<Entry>::Segment &segment, DashTable<Entry>::Segment::iterator it, 
                     std::string_view key, MemoryResource *resource)
          : segment_{ segment}, 
            it_{ it }, 
            key_{ key }, 
            resource_{ resource } 
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
                // 直接传入resource_，避免用户手动传入，减少出错的可能性
                .value = T{ std::forward<Args>(args)..., resource_ },
                .expire_at = TTLManager::PERSISTENT_INTEGRAL
            };

            auto [iter, emplaced] = segment_.emplace(Key{ key_, resource_ }, std::move(entry));
            it_ = iter;
            return std::get_if<T>(&it_->second.value);
        }

        template<typename T, typename... Args>
        auto update(std::type_identity<T>, Args&&... args) -> bool
        {
            if (!exists())
                return false;

            it_->second.value = T{ std::forward<Args>(args)... };
            return std::get_if<T>(&it_->second.value) != nullptr;
        }

        auto expire(int64_t ms) -> bool
        {
            if (!exists()) return false;

            it_->second.expire_at = TTLManager::expire_at(ms);
            return true;
        }

        auto persist() -> bool
        {
            if (!exists()) return false;

            it_->second.expire_at = TTLManager::PERSISTENT_INTEGRAL;
            return true;
        }
        
        auto ttl() -> std::optional<int64_t>
        {
            if (!exists()) return {};

            auto expire_at = it_->second.expire_at;
            if (TTLManager::is_persist(expire_at))
                return TTLManager::PERSISTENT_INTEGRAL;

            if (TTLManager::is_expired(expire_at)) {
                segment_.erase(it_);
                it_ = segment_.end();
                return {};
            }

            return TTLManager::ttl(expire_at);
        }

    private:
        DashTable<Entry>::Segment& segment_;
        DashTable<Entry>::Segment::iterator it_;
        std::string_view key_;
        MemoryResource* resource_;
    };

    explicit DBShard(MemoryResource* resource);

    template<typename Func>
    auto access(std::string_view key, Func&& func) -> decltype(auto)
    {
        // 只读路径，内部是shared_lock，允许并发访问
        return tables_.access(key, [&] (auto& segment, auto it) {
            if (it != segment.end() && ttl_manager_.is_expired(it->second.expire_at)) {
                segment.erase(it);
                it = segment.end();
            }

            EntryHandler handler{ segment, it, key, resource_ };
            return func(handler);
        });
    }

    template<typename Func>
    auto modify(std::string_view key, Func&& func) -> decltype(auto)
    {
        // 修改路径，内部是unique_lock，保证独占访问
        return tables_.modify(key, [&] (auto& segment, auto it) {
            if (it != segment.end() && ttl_manager_.is_expired(it->second.expire_at)) {
                segment.erase(it);
                it = segment.end();
            }

            EntryHandler handler{ segment, it, key, resource_ };
            return func(handler);
        });
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return tables_.size();
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        return tables_.empty();
    }

private:
    MemoryResource* resource_;
    DashTable<Entry> tables_;
    TTLManager ttl_manager_;

    auto erase_if_expired(std::string_view key) -> bool;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_DB_SHARD_H
