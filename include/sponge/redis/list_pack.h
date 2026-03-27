#ifndef SPONGE_REDIS_LIST_PACK_H
#define SPONGE_REDIS_LIST_PACK_H

#include <cstddef>
#include <cstring>
#include <memory_resource>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace spg::redis {

/*
* |<-------- header (6 bytes) ------->|<----- entries ----->| EOF |
* header:
*   total-bytes : 4 bytes (uint32)
*   num-elements: 2 bytes (uint16, 65535 表示未知)
*
* EOF:
*   0xFF
*
* entries:
* [encoding + (len)] [payload] [backlen]
* 
*/
class ListPack {
public:
    class Iterator;
    class ReverseIterator;
    using Size = size_t;

    ListPack(Size capacity, std::pmr::memory_resource* resource);

    explicit ListPack(Size capacity);

    void push_back(std::string_view view);

    void push_back(int64_t value);

    template<std::integral T>
    void push_back(T value)
    {
        push_back(static_cast<int64_t>(value));
    }

    void insert(Iterator pos, std::string_view view);

    void insert(Iterator pos, int64_t value);

    template<typename Iter, std::integral T>
        requires std::is_same_v<Iter, Iterator>
    void insert(Iter pos, T value)
    {
        insert(pos, static_cast<int64_t>(value));
    }

    void erase(Iterator pos);

    void erase(Iterator first, Iterator last);

    void clear() noexcept;

    [[nodiscard]]
    auto size() const noexcept -> Size
    {
        return num_elements();
    }
    
    [[nodiscard]]
    auto empty() const noexcept -> bool
    {
        return size() == 0;
    }

    [[nodiscard]]
    constexpr auto byte_size() const noexcept -> Size
    {
        return buffer_.size();
    }

    [[nodiscard]]
    constexpr auto capacity() const noexcept -> Size
    {
        return buffer_.capacity();
    }

    [[nodiscard]]
    auto view() const noexcept -> std::string_view
    {
        return { reinterpret_cast<const char*>(buffer_.data()), buffer_.size() };
    }

    [[nodiscard]]
    auto begin() noexcept -> Iterator;

    [[nodiscard]]
    auto end() noexcept -> Iterator;

    [[nodiscard]]
    auto rbegin() noexcept -> ReverseIterator;

    [[nodiscard]]
    auto rend() noexcept -> ReverseIterator;

private:
    static constexpr Size HEADER_SIZE = 6;
    static constexpr std::byte END_OF_BUFFER = static_cast<std::byte>(0b11111111);
    using TotalBytes = uint32_t;
    using NumElements = uint16_t;
    using Container = std::pmr::vector<std::byte>;

    std::pmr::memory_resource* resource_;
    Container buffer_{ resource_ };

    void update_total_bytes() noexcept;
    
    void increase_num_elements(NumElements delta) noexcept;

    void decrease_num_elements(NumElements delta) noexcept;

    void num_elements(NumElements value) noexcept;

    [[nodiscard]]
    auto num_elements() const noexcept -> NumElements;

    void increase_capacity(Size increment);
};


class ListPack::Iterator {
    friend class ListPack;

public:
    using BufferIterator = Container::iterator;
    using Element = std::variant<int64_t, std::string_view>;

    explicit Iterator(BufferIterator byte) 
      : iter_{ byte } 
    {}

    auto operator==(const Iterator& other) const -> bool
    {
        return iter_ == other.iter_;
    }

    auto operator!=(const Iterator& other) const -> bool
    {
        return !(*this == other);
    }

    auto operator*() const -> Element;

    auto operator++() -> Iterator&;

    auto operator++(int) -> Iterator;

    auto operator--() -> Iterator&;

    auto operator--(int) -> Iterator;

private:
    // 要么指向一个元素的起始位置，要么指向 EOF（END_OF_BUFFER）
    BufferIterator iter_;
};


class ListPack::ReverseIterator {
public:
    using BufferIterator = Container::iterator;
    using Element = std::variant<int64_t, std::string_view>;

    explicit ReverseIterator(BufferIterator byte) 
      : iter_{ byte } 
    {}

    auto operator==(const ReverseIterator& other) const -> bool
    {
        return iter_ == other.iter_;
    }

    auto operator!=(const ReverseIterator& other) const -> bool
    {
        return !(*this == other);
    }

    auto operator*() const -> Element;

    auto operator++() -> ReverseIterator&;

    auto operator++(int) -> ReverseIterator;

    auto operator--() -> ReverseIterator&;

    auto operator--(int) -> ReverseIterator;

private:
    BufferIterator iter_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_LIST_PACK_H
