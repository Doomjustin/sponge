#ifndef SPONGE_LEVELDB_CODING_H
#define SPONGE_LEVELDB_CODING_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace spg::leveldb {

struct fixed {
    fixed() = delete;

    template<std::unsigned_integral T>
    static void write(std::span<std::byte> dst, T value)
    {
        std::memcpy(dst.data(), &value, sizeof(value));
    }

    template<std::unsigned_integral T>
    static auto read(std::span<const std::byte> src) -> T
    {
        T value{};
        std::memcpy(&value, src.data(), sizeof(value));
        return value;
    }

    template<typename T>
        requires std::is_scoped_enum_v<T>
    static void append(std::vector<std::byte>& dst, T value)
    {
        append(dst, std::to_underlying(value));
    }

    template<std::unsigned_integral T>
    static void append(std::vector<std::byte>& dst, T value)
    {
        size_t old_size = dst.size();
        dst.resize(old_size + sizeof(value));
        std::memcpy(dst.data() + old_size, &value, sizeof(value));
    }

    template<std::unsigned_integral T>
    static void append(std::string& dst, T value)
    {
        size_t old_size = dst.size();
        dst.resize(old_size + sizeof(value));
        std::memcpy(dst.data() + old_size, &value, sizeof(value));
    }

    static void append(std::vector<std::byte>& dst, std::string_view value)
    {
        size_t old_size = dst.size();
        dst.resize(old_size + value.size());
        std::memcpy(dst.data() + old_size, value.data(), value.size());
    }
};

struct varint {
    varint() = delete;

    template<std::unsigned_integral T>
    using ReadResult = std::pair<T, size_t>;

    template<std::unsigned_integral T>
    static void append(std::vector<std::byte>& dst, T value)
    {
        while (value >= 128) {
            dst.push_back(static_cast<std::byte>(value | 0x80));
            value >>= 7;
        }

        dst.push_back(static_cast<std::byte>(value));
    }

    template<std::unsigned_integral T>
    static void append(std::string& dst, T value)
    {
        while (value >= 128) {
            dst.push_back(static_cast<char>(value | 0x80));
            value >>= 7;
        }

        dst.push_back(static_cast<char>(value));
    }

    template<std::unsigned_integral T>
    static auto read(std::span<const std::byte> src) -> std::optional<ReadResult<T>>
    {
        constexpr size_t max_bytes = (std::numeric_limits<T>::digits + 6) / 7;

        T value{};
        size_t shift = 0;

        for (size_t i = 0; i < src.size(); ++i) {
            if (i >= max_bytes)
                return {};

            auto byte = std::to_integer<uint8_t>(src[i]);
            auto chunk = static_cast<T>(byte & 0x7FU);

            value |= static_cast<T>(chunk << shift);
            if ((byte & 0x80U) == 0)
                return ReadResult<T>{ value, i + 1 };

            shift += 7;
        }

        return {};
    }

    template<std::unsigned_integral T>
    static auto read(std::string_view src) -> std::optional<ReadResult<T>>
    {
        constexpr size_t max_bytes = (std::numeric_limits<T>::digits + 6) / 7;

        T value{};
        size_t shift = 0;

        for (size_t i = 0; i < src.size(); ++i) {
            if (i >= max_bytes)
                return {};

            auto byte = static_cast<uint8_t>(static_cast<unsigned char>(src[i]));
            auto chunk = static_cast<T>(byte & 0x7FU);

            value |= static_cast<T>(chunk << shift);
            if ((byte & 0x80U) == 0)
                return ReadResult<T>{ value, i + 1 };

            shift += 7;
        }

        return {};
    }
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_CODING_H
