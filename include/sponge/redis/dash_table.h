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
#include <sponge/tag.h>

namespace spg::redis {

template<typename Value>
class DashTable {
public:
    using Size = std::size_t;
    using Segment = std::pmr::unordered_map<std::pmr::string, Value, PmrStringHash, std::equal_to<>>;

    class LockedReadView;
    class ReadRange;

    class LockedModifyView;
    class ModifyRange;

    auto mutex_of(std::string_view key) -> std::shared_mutex&
    {
        auto segment_index = index_for(key);
        return locks_[segment_index];
    }

    template<typename Func>
    auto modify(std::string_view key, Func&& func) -> decltype(auto)
    {
        auto segment_index = index_for(key);
        std::unique_lock locker{ locks_[segment_index] };

        auto& segment = datas_[segment_index];
        auto it = segment.find(key);
        return func(segment, it);
    }

    auto get(std::string_view key, UnlockTag unlock) -> Value*
    {
        auto& segment = datas_[index_for(key)];
        auto it = segment.find(key);
        return (it != segment.end()) ? &it->second : nullptr;
    }

    auto get(std::string_view key) -> Value*
    {
        auto segment_index = index_for(key);
        std::unique_lock locker{ locks_[segment_index] };
        auto& segment = datas_[segment_index];
        auto it = segment.find(key);
        return (it != segment.end()) ? &it->second : nullptr;
    }

    void set(std::string_view key, Value value, UnlockTag unlock)
    {
        datas_[index_for(key)].insert_or_assign(std::pmr::string{ key }, std::move(value));
    }

    void set(std::string_view key, Value value)
    {
        auto segment_index = index_for(key);
        std::unique_lock locker{ locks_[segment_index] };
        auto& segment = datas_[segment_index];
        segment.insert_or_assign(std::pmr::string{ key }, std::move(value));
    }

    auto erase(std::string_view key, UnlockTag unlock) -> bool
    {
        auto& segment = datas_[index_for(key)];
        auto it = segment.find(key);
        if (it == segment.end())
            return false;
        segment.erase(it);
        return true;
    }

    auto erase(std::string_view key) -> bool
    {
        auto segment_index = index_for(key);
        std::unique_lock locker{ locks_[segment_index] };
        auto& segment = datas_[segment_index];
        auto it = segment.find(key);
        if (it == segment.end())
            return false;
        segment.erase(it);
        return true;
    }

    void clear()
    {
        for (size_t i = 0; i < SEGMENTS; ++i) {
            std::unique_lock locker{ locks_[i] };
            datas_[i].clear();
        }
    }

    auto range(ReadOnlyTag read_only) const noexcept -> ReadRange
    {
        return ReadRange{ *this };
    }

    auto range(ReadWriteTag read_write) noexcept -> ModifyRange
    {
        return ModifyRange{ *this };
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        Size total = 0;
        for (size_t i = 0; i < SEGMENTS; ++i) {
            std::shared_lock locker{ locks_[i] };
            total += datas_[i].size();
        }

        return total;
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        for (size_t i = 0; i < SEGMENTS; ++i) {
            std::shared_lock locker{ locks_[i] };
            if (!datas_[i].empty())
                return false;
        }

        return true;
    }

private:
    static constexpr Size SEGMENTS = 1024;
    static constexpr Size SEGMENT_MASK = SEGMENTS - 1;

    std::array<Segment, SEGMENTS> datas_;
    mutable std::array<std::shared_mutex, SEGMENTS> locks_;

    [[nodiscard]]
    auto index_for(std::string_view key) const -> Size
    {
        return hash(key, use_fnv_1a) & SEGMENT_MASK;
    }
};


template<typename Value>
class DashTable<Value>::LockedReadView {
public:
    LockedReadView(const Segment& segment, std::shared_mutex& mutex)
      : segment_{ segment },
        locker_{ mutex }
    {}

    auto begin() const noexcept
    {
        return segment_.begin();
    }

    auto end() const noexcept
    {
        return segment_.end();
    }

private:
    const Segment& segment_;
    std::shared_lock<std::shared_mutex> locker_;
};


template<typename Value>
class DashTable<Value>::ReadRange {
public:
    class Iterator {
    public:
        Iterator(size_t index, const DashTable<Value>& table)
          : index_{ index }, table_{ table }
        {}

        auto operator++() -> Iterator&
        {
            ++index_;
            return *this;
        }

        auto operator++(int) -> Iterator
        {
            auto temp = *this;
            ++index_;
            return temp;
        }

        auto operator--() -> Iterator&
        {
            --index_;
            return *this;
        }

        auto operator--(int) -> Iterator
        {
            auto temp = *this;
            --index_;
            return temp;
        }

        auto operator*() const -> LockedReadView
        {
            auto& data = table_.datas_[index_];
            auto& mutex = table_.locks_[index_];
            return LockedReadView{ data, mutex };
        }

        auto operator==(const Iterator& rhs) const noexcept -> bool
        {
            return &table_ == &rhs.table_ && index_ == rhs.index_;
        }

        auto operator!=(const Iterator& rhs) const noexcept -> bool
        {
            return !(*this == rhs);
        }

    private:
        size_t index_;
        const DashTable<Value>& table_;
    };

    ReadRange(const DashTable<Value>& table)
      : table_{ table }
    {}

    auto begin() const -> Iterator
    {
        return Iterator{ 0, table_ };
    }

    auto end() const -> Iterator
    {
        return Iterator{ SEGMENTS, table_ };
    }

private:
    const DashTable<Value>& table_;
};


template<typename Value>
class DashTable<Value>::LockedModifyView {
public:
    LockedModifyView(Segment& segment, std::shared_mutex& mutex)
      : segment_{ segment },
        locker_{ std::unique_lock<std::shared_mutex>(mutex) }
    {}

    auto begin() const noexcept
    {
        return segment_.begin();
    }

    auto end() const noexcept
    {
        return segment_.end();
    }

private:
    Segment& segment_;
    std::unique_lock<std::shared_mutex> locker_;
};


template<typename Value>
class DashTable<Value>::ModifyRange {
public:
    class Iterator {
    public:
        Iterator(size_t index, DashTable<Value>& table)
          : index_{ index }, table_{ table }
        {}

        auto operator++() -> Iterator&
        {
            ++index_;
            return *this;
        }

        auto operator++(int) -> Iterator
        {
            auto temp = *this;
            ++index_;
            return temp;
        }

        auto operator--() -> Iterator&
        {
            --index_;
            return *this;
        }

        auto operator--(int) -> Iterator
        {
            auto temp = *this;
            --index_;
            return temp;
        }

        auto operator*() -> LockedModifyView
        {
            auto& data = table_.datas_[index_];
            auto& mutex = table_.locks_[index_];
            return LockedModifyView{ data, mutex };
        }

        auto operator==(const Iterator& rhs) const noexcept -> bool
        {
            return &table_ == &rhs.table_ && index_ == rhs.index_;
        }

        auto operator!=(const Iterator& rhs) const noexcept -> bool
        {
            return !(*this == rhs);
        }

    private:
        size_t index_;
        DashTable<Value>& table_;
    };

    ModifyRange(DashTable<Value>& table)
      : table_{ table }
    {}

    auto begin() const -> Iterator
    {
        return Iterator{ 0, table_ };
    }

    auto end() const -> Iterator
    {
        return Iterator{ SEGMENTS, table_ };
    }

private:
    DashTable<Value>& table_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_DASH_TABLE_H
