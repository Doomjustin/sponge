#include <sponge/redis/protocol.h>

#include <memory_resource>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

TEST_CASE("parse_resp_batch parses single command", "[spg_redis_protocol][parse]")
{
    auto* resource{ std::pmr::new_delete_resource() };
    std::string_view buf{ "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n" };

    auto result{ resp::parse_request(buf, resource) };

    REQUIRE(result.commands.size() == 1);
    REQUIRE(result.commands[0].size() == 3);
    REQUIRE(result.commands[0][0] == "SET");
    REQUIRE(result.commands[0][1] == "mykey");
    REQUIRE(result.commands[0][2] == "myvalue");
    REQUIRE(result.consumed_bytes == buf.size());
}

TEST_CASE("parse_resp_batch parses multiple commands in one buffer", "[spg_redis_protocol][parse]")
{
    auto* resource{ std::pmr::new_delete_resource() };
    std::string_view buf{ "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$7\r\nmyvalue\r\n"
                          "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n" };

    auto result{ resp::parse_request(buf, resource) };

    REQUIRE(result.commands.size() == 2);
    REQUIRE(result.commands[0][0] == "SET");
    REQUIRE(result.commands[1][0] == "GET");
    REQUIRE(result.consumed_bytes == buf.size());
}

TEST_CASE("parse_resp_batch returns empty on incomplete buffer", "[spg_redis_protocol][partial]")
{
    auto* resource{ std::pmr::new_delete_resource() };
    std::string_view buf{ "*3\r\n$3\r\nSET\r\n$5\r\nmyk" }; // 半包：最后一个参数不完整

    auto result{ resp::parse_request(buf, resource) };

    REQUIRE(result.commands.empty());
    REQUIRE(result.consumed_bytes == 0);
}

TEST_CASE("parse_resp_batch throws on malformed bulk string", "[spg_redis_protocol][malformed]")
{
    auto* resource{ std::pmr::new_delete_resource() };
    std::string_view buf{ "*1\r\nNOT_BULK\r\n" }; // 参数应以 $ 开头

    REQUIRE_THROWS_AS(resp::parse_request(buf, resource), std::runtime_error);
}