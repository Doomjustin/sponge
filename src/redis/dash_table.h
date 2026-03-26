#ifndef SPONGE_REDIS_DASH_TABLE_H
#define SPONGE_REDIS_DASH_TABLE_H

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sponge/hash.h>

namespace spg::redis {

template<typename Value>
class DashTable {
public:
    using Key = std::pmr::string;
    using Size = std::size_t;
    using MemoryResource = std::pmr::memory_resource;

    explicit DashTable(MemoryResource* resource)
      : resource_{ resource }
    {
        for (auto& segment : datas_)
            segment = Container{ resource };
    }

    auto get(std::string_view key) -> Value*
    {
        auto& segment = datas_[hash(key, use_fnv_1a) & SEGMENT_MASK];
        auto it = segment.find(key);
        return (it != segment.end()) ? &it->second : nullptr;
    }

    void set(std::string_view key, Value value)
    {
        auto& segment = datas_[hash(key, use_fnv_1a) & SEGMENT_MASK];
        segment.insert_or_assign(Key{ key, resource_ }, std::move(value));
    }

    auto erase(std::string_view key) -> bool
    {
        auto& segment = datas_[hash(key, use_fnv_1a) & SEGMENT_MASK];
        return segment.erase(Key{ key, resource_ }) > 0;
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        Size total = 0;
        for (const auto& segment : datas_)
            total += segment.size();

        return total;
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        for (const auto& segment : datas_) {
            if (!segment.empty())
                return false;
        }

        return true;
    }

private:
    using Container = std::pmr::unordered_map<Key, Value, PmrStringHash, std::equal_to<>>;

    static constexpr Size SEGMENTS = 1024;
    static constexpr Size SEGMENT_MASK = SEGMENTS - 1;

    MemoryResource* resource_;
    std::array<Container, SEGMENTS> datas_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_DASH_TABLE_H
