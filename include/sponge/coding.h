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

    template<typename Iterator, std::unsigned_integral T>
        requires std::output_iterator<Iterator, std::byte>
    static void encode(Iterator iter, T value) noexcept
    {
        while (value > MASK) {
            // 写入一个字节，最高位为1表示后面还有字节
            auto byte = static_cast<std::byte>((value & MASK) | CONTINUATION);
            *iter++ = byte; 
            value >>= SHIFT_BITS;
        }
        
        *iter++ = static_cast<std::byte>(value); // 最后一个字节，最高位为0
    }

    template<typename Iterator, std::unsigned_integral T>
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

private:
    static constexpr int SHIFT_BITS = 7;
    static constexpr auto MASK = 0b01111111;
    static constexpr auto CONTINUATION = 0b10000000;

    static auto is_continuation(uint8_t byte) noexcept -> bool
    {
        return byte & CONTINUATION;
    }
};

} // namespace spg

#endif // SPONGE_CODING_H
