#include "protocol.h"

#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

TEST_CASE("解析RESP批量请求时应解析单个命令", "[redis][protocol]")
{
    std::string_view request_buffer{ "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n" };

    auto result{ resp::parse_request(request_buffer) };

    REQUIRE(result.commands.size() == 1);
    REQUIRE(result.commands[0].arguments.size() == 3);
    REQUIRE(result.commands[0].arguments[0] == "SET");
    REQUIRE(result.commands[0].arguments[1] == "mykey");
    REQUIRE(result.commands[0].arguments[2] == "myvalue");
    REQUIRE(result.commands[0].raw == "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n");
    REQUIRE(result.consumed_bytes == request_buffer.size());
}

TEST_CASE("解析RESP批量请求时应解析同一缓冲区中的多个命令", "[redis][protocol]")
{
    std::string_view request_buffer{ "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n"
                                     "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n" };

    auto result{ resp::parse_request(request_buffer) };

    REQUIRE(result.commands.size() == 2);
    REQUIRE(result.commands[0].arguments[0] == "SET");
    REQUIRE(result.commands[1].arguments[0] == "GET");
    REQUIRE(result.commands[0].raw == "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n");
    REQUIRE(result.commands[1].raw == "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n");
    REQUIRE(result.consumed_bytes == request_buffer.size());
}

TEST_CASE("解析RESP批量请求时应返回空结果", "[redis][protocol]")
{
    std::string_view request_buffer{ "*3\r\n$3\r\nSET\r\n$5\r\nmyk" }; // 半包：最后一个参数不完整

    auto result{ resp::parse_request(request_buffer) };

    REQUIRE(result.commands.empty());
    REQUIRE(result.consumed_bytes == 0);
}

TEST_CASE("解析RESP批量请求在格式错误的批量字符串场景应抛出异常", "[redis][protocol]")
{
    std::string_view request_buffer{ "*1\r\nNOT_BULK\r\n" }; // 参数应以 $ 开头

    REQUIRE_THROWS_AS(resp::parse_request(request_buffer), std::runtime_error);
}
