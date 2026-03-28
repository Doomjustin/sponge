#include "protocol.h"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace spg::redis {

auto find_crlf(const char* start, const char* end) -> const char*
{
    std::string_view view{ start, static_cast<std::size_t>(end - start) };
    auto pos = view.find("\r\n");
    return (pos == std::string_view::npos) ? nullptr : (start + pos);
}

struct BulkParseResult {
    bool partial = false;
    std::string_view value;
    const char* next = nullptr;
};

// expected: $3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n
auto parse_bulk_string(const char* begin, const char* const end) -> BulkParseResult
{
    if (begin >= end)
        return { .partial = true }; // 半包：连头部都没有

    if (*begin != '$')
        throw std::runtime_error{ "Malformed RESP bulk string header" };

    const auto* crlf = find_crlf(begin, end);
    if (!crlf)
        return { .partial = true }; // 半包：$N 后面的 \r\n 还没到

    int bulk_len;
    auto [p, ec] = std::from_chars(begin + 1, crlf, bulk_len);
    if (ec != std::errc() || p != crlf || bulk_len < 0)
        throw std::runtime_error{ "Malformed RESP bulk string header" };

    const auto* payload = crlf + 2; // 跳过CRLF

    // 这是一个半包的情况，等待下一次数据到来
    if (end - payload < bulk_len + 2)
        return { .partial = true };

    const auto* value_end = payload + bulk_len;
    if (value_end[0] != '\r' || value_end[1] != '\n')
        throw std::runtime_error{ "Malformed RESP bulk string body" };

    return BulkParseResult{
        .partial = false,
        .value = std::string_view{ payload, static_cast<std::size_t>(bulk_len) },
        .next = value_end + 2,
    };
}

// expected: *3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n
auto parse_array_command(const char* begin, const char* const end, std::pmr::memory_resource* resource) -> resp::Command::Arguments
{
    resp::Command::Arguments args{ resource };

    assert(*begin == '*');

    const auto* crlf = find_crlf(begin, end);
    if (!crlf)
        return {}; // 半包：*N 后面的 \r\n 还没到

    int argc = 0;
    auto [p, ec] = std::from_chars(begin + 1, crlf, argc);
    if (ec != std::errc() || p != crlf || argc <= 0)
        throw std::runtime_error{ "Malformed RESP array header" };

    begin = crlf + 2; // 跳过CRLF

    for (int i = 0; i < argc; ++i) {
        auto bulk = parse_bulk_string(begin, end);
        // 如果是半包，结束当前解析，等待下一次数据到来
        if (bulk.partial)
            return {};

        args.emplace_back(bulk.value);
        begin = bulk.next;
    }

    return args;
}

auto resp::parse_request(std::string_view buffer, std::pmr::memory_resource* resource) -> ParseResult
{
    ParseResult result{ resource };
    const auto* ptr = buffer.data();
    const auto* const end = buffer.data() + buffer.size();

    while (ptr < end) {
        const auto* raw_start = ptr;

        if (*ptr == '*') {
            auto command = parse_array_command(ptr, end, resource);

            if (command.empty())
                break;

            auto last_command = command.back();
            ptr = last_command.data() + last_command.size() + 2; // 跳过最后一个参数的CRLF

            std::string_view raw{ raw_start, static_cast<std::size_t>(ptr - raw_start) };
            result.commands.emplace_back(std::move(command), raw);
            result.consumed_bytes = static_cast<std::size_t>(ptr - buffer.data());
        }
        else {
            // 如果不是*开头，说明协议就错了，直接丢弃这个包
            const auto* crlf = find_crlf(ptr, end);
            if (!crlf)
                break;

            ptr = crlf + 2; // 跳过CRLF
        }
    }

    return result;
}

} // namespace spg::redis
