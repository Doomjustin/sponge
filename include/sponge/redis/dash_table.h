#ifndef SPONGE_REDIS_DASH_TABLE_H
#define SPONGE_REDIS_DASH_TABLE_H

#include <array>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sponge/hash.h>

namespace spg::redis {

struct UnlockT {};

inline constexpr UnlockT unlock{};

template<typename Value>
class DashTable {
public:
    using Key = std::pmr::string;
    using Size = std::size_t;
    using MemoryResource = std::pmr::memory_resource;
    using Segment = std::pmr::unordered_map<Key, Value, PmrStringHash, std::equal_to<>>;

    explicit DashTable(MemoryResource* resource)
      : resource_{ resource }
    {
        for (auto& segment : datas_)
            segment = Segment{ resource };
    }

    auto mutex_of(std::string_view key) -> std::shared_mutex&
    {
        auto segment_index = index_for(key);
        return locks_[segment_index];
    }

    template<typename Func>
    auto modify(std::string_view key, Func&& func) -> decltype(auto)
    {
        auto segment_index = index_for(key);
        std::unique_lock<std::shared_mutex> locker{ locks_[segment_index] };

        auto& segment = datas_[segment_index];
        auto it = segment.find(key);
        return func(segment, it);
    }

    auto get(std::string_view key, UnlockT unlock) -> Value*
    {
        auto& segment = datas_[index_for(key)];
        auto it = segment.find(key);
        return (it != segment.end()) ? &it->second : nullptr;
    }

    auto get(std::string_view key) -> Value*
    {
        auto segment_index = index_for(key);
        std::unique_lock<std::shared_mutex> locker{ locks_[segment_index] };
        auto& segment = datas_[segment_index];
        auto it = segment.find(key);
        return (it != segment.end()) ? &it->second : nullptr;
    }

    void set(std::string_view key, Value value, UnlockT unlock)
    {
        datas_[index_for(key)].insert_or_assign(Key{ key, resource_ }, std::move(value));
    }

    void set(std::string_view key, Value value)
    {
        auto segment_index = index_for(key);
        std::unique_lock<std::shared_mutex> locker{ locks_[segment_index] };
        auto& segment = datas_[segment_index];
        segment.insert_or_assign(Key{ key, resource_ }, std::move(value));
    }

    auto erase(std::string_view key, UnlockT unlock) -> bool
    {
        return datas_[index_for(key)].erase(Key{ key, resource_ }) > 0;
    }

    auto erase(std::string_view key) -> bool
    {
        auto segment_index = index_for(key);
        std::unique_lock<std::shared_mutex> locker{ locks_[segment_index] };
        auto& segment = datas_[segment_index];
        return segment.erase(Key{ key, resource_ }) > 0;
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        Size total = 0;
        for (size_t i = 0; i < SEGMENTS; ++i) {
            std::shared_lock<std::shared_mutex> locker{ locks_[i] };
            total += datas_[i].size();
        }

        return total;
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        for (size_t i = 0; i < SEGMENTS; ++i) {
            std::shared_lock<std::shared_mutex> locker{ locks_[i] };
            if (!datas_[i].empty())
                return false;
        }

        return true;
    }

private:
    static constexpr Size SEGMENTS = 1024;
    static constexpr Size SEGMENT_MASK = SEGMENTS - 1;

    MemoryResource* resource_;
    std::array<Segment, SEGMENTS> datas_;
    mutable std::array<std::shared_mutex, SEGMENTS> locks_;

    [[nodiscard]]
    auto index_for(std::string_view key) const -> Size
    {
        return hash(key, use_fnv_1a) & SEGMENT_MASK;
    }
};

} // namespace spg::redis

#endif // SPONGE_REDIS_DASH_TABLE_H
