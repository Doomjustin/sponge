#include "reply.h"

#include <forward_list>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

TEST_CASE("构造回复时应编码标量RESP类型", "[redis][reply]")
{
    Reply reply;
    reply.append(42)
        .append(SimpleString{ "PONG" })
        .append(Error{ "ERR failed" })
        .append(BulkString{ "hello" })
        .append(BulkString{ "" })
        .append(ArrayHeader{ 3 })
        .append(NullString{})
        .append(NullArray{})
        .append(OK{});

    REQUIRE(reply.str() == ":42\r\n"
                           "+PONG\r\n"
                           "-ERR failed\r\n"
                           "$5\r\nhello\r\n"
                           "$0\r\n\r\n"
                           "*3\r\n"
                           "$-1\r\n"
                           "*-1\r\n"
                           "+OK\r\n");
}

TEST_CASE("构造回复时应追加定长RespRange", "[redis][reply]")
{
    std::vector<std::string_view> items{ "GET", "mykey" };

    Reply reply;
    reply.append(RespRange{ items });

    REQUIRE(reply.str() == "*2\r\n"
                           "$3\r\nGET\r\n"
                           "$5\r\nmykey\r\n");
}

TEST_CASE("构造回复时应追加非定长RespRange", "[redis][reply]")
{
    std::forward_list<int> items{ 1, 2, 3 };

    Reply reply;
    reply.append(RespRange{ items });

    REQUIRE(reply.str() == "*3\r\n"
                           ":1\r\n"
                           ":2\r\n"
                           ":3\r\n");
}

TEST_CASE("构造回复时应追加扁平数组形式的RespMap", "[redis][reply]")
{
    std::vector<std::pair<std::string_view, int>> items{
        { "first", 7 },
        { "second", 9 },
    };

    Reply reply;
    reply.append(RespMap<decltype((items))>{ items });

    REQUIRE(reply.str() == "*4\r\n"
                           "$5\r\nfirst\r\n"
                           ":7\r\n"
                           "$6\r\nsecond\r\n"
                           ":9\r\n");
}

TEST_CASE("构造回复可变参数追加时应前置数组头", "[redis][reply]")
{
    Reply reply;
    reply.append(BulkString{ "SET" }, BulkString{ "key" }, BulkString{ "value" });

    REQUIRE(reply.str() == "*3\r\n"
                           "$3\r\nSET\r\n"
                           "$3\r\nkey\r\n"
                           "$5\r\nvalue\r\n");
}
