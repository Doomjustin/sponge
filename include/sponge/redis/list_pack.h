#ifndef SPONGE_REDIS_LIST_PACK_H
#define SPONGE_REDIS_LIST_PACK_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
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

    explicit ListPack(size_t capacity);

    void push_back(std::string_view view);

    void push_back_string(std::string_view view);

    void push_back(int64_t value);

    template<std::integral T>
    void push_back(T value)
    {
        push_back(static_cast<int64_t>(value));
    }

    template<std::floating_point T>
    void push_back(T value)
    {
        push_back(std::to_string(value));
    }

    auto insert(Iterator pos, std::string_view view) -> Iterator;

    auto insert_string(Iterator pos, std::string_view view) -> Iterator;

    auto insert(Iterator pos, int64_t value) -> Iterator;

    template<typename Iter, std::integral T>
        requires std::is_same_v<Iter, Iterator>
    void insert(Iter pos, T value)
    {
        insert(pos, static_cast<int64_t>(value));
    }

    template<typename Iter, std::floating_point T>
        requires std::is_same_v<Iter, Iterator>
    void insert(Iter pos, T value)
    {
        insert(pos, std::to_string(value));
    }

    void erase(Iterator pos);

    void erase(Iterator first, Iterator last);

    void clear() noexcept;

    [[nodiscard]]
    auto size() const noexcept -> size_t
    {
        return num_elements();
    }
    
    [[nodiscard]]
    auto empty() const noexcept -> bool
    {
        return size() == 0;
    }

    [[nodiscard]]
    constexpr auto byte_size() const noexcept -> size_t
    {
        return buffer_.size();
    }

    [[nodiscard]]
    constexpr auto capacity() const noexcept -> size_t
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
    auto begin() const noexcept -> Iterator;

    [[nodiscard]]
    auto end() noexcept -> Iterator;

    [[nodiscard]]
    auto end() const noexcept -> Iterator;

    [[nodiscard]]
    auto rbegin() noexcept -> ReverseIterator;

    [[nodiscard]]
    auto rbegin() const noexcept -> ReverseIterator;

    [[nodiscard]]
    auto rend() noexcept -> ReverseIterator;

    [[nodiscard]]
    auto rend() const noexcept -> ReverseIterator;

private:
    static constexpr size_t HEADER_SIZE = 6;
    static constexpr std::byte END_OF_BUFFER = static_cast<std::byte>(0b11111111);
    using TotalBytes = uint32_t;
    using NumElements = uint16_t;

    std::pmr::vector<std::byte> buffer_{};

    void update_total_bytes() noexcept;
    
    void increase_num_elements(NumElements delta) noexcept;

    void decrease_num_elements(NumElements delta) noexcept;

    void num_elements(NumElements value) noexcept;

    [[nodiscard]]
    auto num_elements() const noexcept -> NumElements;

    void increase_capacity(size_t increment);
};


class ListPack::Iterator {
    friend class ListPack;

public:
    using BufferIterator = std::pmr::vector<std::byte>::iterator;
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
    using BufferIterator = std::pmr::vector<std::byte>::iterator;
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
