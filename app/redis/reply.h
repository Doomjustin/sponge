#ifndef SPONGE_REDIS_REPLY_H
#define SPONGE_REDIS_REPLY_H

#include <iterator>
#include <ranges>
#include <string_view>
#include <type_traits>

#include <boost/asio.hpp>
#include <fmt/format.h>

namespace spg::redis {

struct SimpleString { std::string_view value; };
struct Error{ std::string_view message; };
struct BulkString { std::string_view value; };
struct ArrayHeader { size_t length; };
struct NullString {};
struct NullArray {};
struct OK {};

template<std::ranges::range Range>
struct RespRange {
    Range&& range;
};

template<std::ranges::range Range>
RespRange(Range&&) -> RespRange<Range>;

template<typename T>
struct is_resp_range: std::false_type {};

template<typename Range>
struct is_resp_range<RespRange<Range>>: std::true_type {};

template<typename Range>
inline constexpr bool is_resp_range_v = is_resp_range<Range>::value;

template<std::ranges::range Range>
struct RespMap {
    Range&& range;
};

template<std::ranges::range Range>
RespMap(Range&&) -> RespMap<Range>;

template<typename T>
struct is_resp_map: std::false_type {};

template<typename Range>
struct is_resp_map<RespMap<Range>>: std::true_type {};

template<typename Range>
inline constexpr bool is_resp_map_v = is_resp_map<Range>::value;

inline constexpr NullString null_string{};
inline constexpr NullArray null_array{};
inline constexpr OK ok{};


class Reply {
public:
    template<typename T>
    auto append(T&& arg) -> Reply&
    {
        using Decayed = std::decay_t<T>;
        auto iter = std::back_inserter(buffer_);

        if constexpr (std::is_integral_v<Decayed>) {
            fmt::format_to(iter, ":{}\r\n", arg);
        }
        else if constexpr (std::is_same_v<Decayed, SimpleString>) {
            fmt::format_to(iter, "+{}\r\n", arg.value);
        }
        else if constexpr (std::is_same_v<Decayed, Error>) {
            fmt::format_to(iter, "-{}\r\n", arg.message);
        }
        else if constexpr (std::is_same_v<Decayed, BulkString>) {
            if (arg.value.empty())
                fmt::format_to(iter, "$0\r\n\r\n");
            else
                fmt::format_to(iter, "${}\r\n{}\r\n", arg.value.size(), arg.value);
        }
        else if constexpr (std::is_same_v<Decayed, ArrayHeader>) {
            fmt::format_to(iter, "*{}\r\n", arg.length);
        }
        else if constexpr (std::is_same_v<Decayed, NullString>) {
            fmt::format_to(iter, "$-1\r\n");
        }
        else if constexpr (std::is_same_v<Decayed, NullArray>) {
            fmt::format_to(iter, "*-1\r\n");
        }
        else if constexpr (std::is_same_v<Decayed, OK>) {
            fmt::format_to(iter, "+OK\r\n");
        }
        else if constexpr (is_resp_range_v<Decayed>) {
            if constexpr (std::ranges::sized_range<decltype(arg.range)>) {
                fmt::format_to(iter, "*{}\r\n", std::ranges::size(arg.range));
            }
            else {
                auto dist = std::ranges::distance(arg.range);
                fmt::format_to(iter, "*{}\r\n", dist);
            }

            for (auto&& item: arg.range) {
                if constexpr (std::is_convertible_v<decltype(item), std::string_view>)
                    append(BulkString{ item });
                else
                    append(item);
            }
        }
        else if constexpr (is_resp_map_v<Decayed>) {
            auto count = std::ranges::size(arg.range);
            fmt::format_to(iter, "*{}\r\n", count * 2); // 每个键值对占两个元素

            for (auto&& [key, value]: arg.range) {
                append(BulkString{ key });
                append(value);
            }
        }
        else {
            static_assert(false, "Unsupported reply type");
        }

        return *this;
    }

    // 这个重载用于批量追加多个元素，适用于 Array 类型的回复
    template<typename... Args>
        requires (sizeof...(Args) > 2)
    auto append(Args&&... args) -> Reply&
    {
        append(ArrayHeader{ sizeof...(Args) });
        // 展开参数包，逐个追加元素
        (append(std::forward<Args>(args)), ...);
        return *this;
    }

    [[nodiscard]]
    auto str() const -> std::string
    {
        return std::string{ buffer_.data(), buffer_.size() };
    }

    [[nodiscard]]
    auto view_buffer() const -> boost::asio::const_buffer
    {
        return boost::asio::const_buffer{ buffer_.data(), buffer_.size() };
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        return buffer_.size() == 0;
    }

    void clear()
    {
        buffer_.clear();
    }

private:
    fmt::memory_buffer buffer_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_REPLY_H
