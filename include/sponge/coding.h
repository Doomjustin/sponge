#ifndef SPONGE_CODING_H
#define SPONGE_CODING_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

namespace spg {

struct varint {
    varint() = delete;

    template<std::unsigned_integral T>
    static auto length(T value) noexcept -> size_t
    {
        size_t result = 0;
        while (value >= 128) {
            value >>= 7;
            ++result;
        }

        return result;
    }

    template<typename Iterator, std::unsigned_integral T>
        requires std::output_iterator<Iterator, std::byte>
    static auto encode(Iterator iter, T value) noexcept -> Iterator
    {
        while (value > MASK) {
            // 写入一个字节，最高位为1表示后面还有字节
            auto byte = static_cast<std::byte>((value & MASK) | CONTINUATION);
            *iter++ = byte;
            value >>= SHIFT_BITS;
        }

        *iter++ = static_cast<std::byte>(value); // 最后一个字节，最高位为0
        return iter;
    }

    template<typename Iterator, std::unsigned_integral T>
        requires std::output_iterator<Iterator, char>
    static auto encode(Iterator iter, T value) noexcept -> Iterator
    {
        while (value > MASK) {
            auto byte = static_cast<char>((value & MASK) | CONTINUATION);
            *iter++ = byte;
            value >>= SHIFT_BITS;
        }

        *iter++ = static_cast<char>(value); // 最后一个字节，最高位为0
        return iter;
    }

    template<std::unsigned_integral T, std::input_iterator Iterator>
        requires std::convertible_to<typename Iterator::value_type, std::byte>
    static auto decode(Iterator iter) noexcept -> std::pair<T, Iterator>
    {
        T value = 0;
        int shift = 0;
        while (true) {
            auto byte_value = std::to_integer<T>(*iter++);
            value |= (byte_value & MASK) << shift;

            if (!is_continuation(byte_value))
                break;

            shift += SHIFT_BITS;
        }

        return { value, iter };
    }

    template<std::unsigned_integral T, std::input_iterator Iterator>
        requires std::convertible_to<typename Iterator::value_type, char> ||
                 std::is_same_v<Iterator, const char*>
    static auto decode(Iterator iter) noexcept -> std::pair<T, Iterator>
    {
        T value = 0;
        int shift = 0;
        while (true) {
            const auto byte_value = static_cast<uint8_t>(*iter++);
            value |= (byte_value & MASK) << shift;

            if (!is_continuation(byte_value))
                break;

            shift += SHIFT_BITS;
        }

        return { value, iter };
    }

private:
    static constexpr int SHIFT_BITS = 7;
    static constexpr auto MASK = 0b01111111;
    static constexpr auto CONTINUATION = 0b10000000;

    static auto is_continuation(const uint8_t byte) noexcept -> bool
    {
        return byte & CONTINUATION;
    }
};

struct fixed {
    fixed() = delete;

    template<std::unsigned_integral T, std::output_iterator<std::byte> Iterator>
    static auto encode(Iterator iter, T value) noexcept -> Iterator
    {
        for (size_t i = 0; i < sizeof(T); ++i) {
            *iter++ = static_cast<std::byte>(value & 0xFF);
            value >>= 8;
        }

        return iter;
    }

    template<typename Iterator, std::unsigned_integral T>
        requires std::output_iterator<Iterator, char>
    static auto encode(Iterator iter, T value) noexcept -> Iterator
    {
        for (size_t i = 0; i < sizeof(T); ++i) {
            *iter++ = static_cast<char>(value & 0xFF);
            value >>= 8;
        }

        return iter;
    }

    template<std::unsigned_integral T, std::input_iterator Iterator>
        requires std::convertible_to<typename Iterator::value_type, std::byte>
    static auto decode(Iterator iter) noexcept -> std::pair<T, Iterator>
    {
        T value = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            value |= (static_cast<T>(std::to_integer<uint8_t>(*iter++)) << (8 * i));

        return { value, iter };
    }

    template<std::unsigned_integral T, std::input_iterator Iterator>
        requires std::convertible_to<typename Iterator::value_type, char> ||
                 std::is_same_v<Iterator, const char*>
    static auto decode(Iterator iter) noexcept -> std::pair<T, Iterator>
    {
        T value = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            value |= (static_cast<T>(static_cast<uint8_t>(*iter++)) << (8 * i));

        return { value, iter };
    }
};


template<uint8_t N, std::unsigned_integral T>
struct PackBytesT {
    T value;
};

template<uint8_t N, std::unsigned_integral T>
constexpr auto pack_bytes(T value) noexcept -> PackBytesT<N, T>
{
    return PackBytesT<N, T>{ value };
}

template<std::unsigned_integral T, std::unsigned_integral U, uint8_t N>
    requires (N > 0 && N < sizeof(T) && N <= sizeof(U))
constexpr auto pack(T dest, PackBytesT<N, U> src) noexcept -> T
{
    constexpr auto shift = 8 * N;
    constexpr auto mask = (T{ 1 } << shift) - 1;
    return (dest << shift) | (static_cast<T>(src.value) & mask);
}

template<std::unsigned_integral U, uint8_t N, std::unsigned_integral T>
    requires (N > 0 && N < sizeof(T) && N <= sizeof(U))
constexpr auto unpack(const T& packed_value) noexcept -> std::pair<U, T>
{
    constexpr auto shift = 8 * N;
    constexpr auto mask = (T{ 1 } << shift) - 1;
    const auto extracted = static_cast<U>(packed_value & mask);
    const auto remaining = packed_value >> shift;
    return { extracted, remaining };
}

} // namespace spg

#endif // SPONGE_CODING_H
