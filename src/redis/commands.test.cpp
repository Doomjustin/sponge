#include "commands.h"

#include <array>
#include <memory_resource>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

TEST_CASE("dispatch handles known command case-insensitively", "[commands]")
{
    DBShard shard{ std::pmr::new_delete_resource() };
    Reply reply;
    Context context{ shard, reply };

    std::array<std::string_view, 3> cmd{ "set", "mykey", "myvalue" };
    commands::dispatch(context, cmd);

    REQUIRE(reply.str() == "+OK\r\n");
}

TEST_CASE("dispatch returns error for unknown command", "[commands]")
{
    DBShard shard{ std::pmr::new_delete_resource() };
    Reply reply;
    Context context{ shard, reply };

    std::array<std::string_view, 1> cmd{ "no_such_cmd" };
    commands::dispatch(context, cmd);

    REQUIRE(reply.str() == "-ERR unknown command 'NO_SUCH_CMD'\r\n");
}

TEST_CASE("dispatch validates argument count for GET", "[commands]")
{
    DBShard shard{ std::pmr::new_delete_resource() };
    Reply reply;
    Context context{ shard, reply };

    std::array<std::string_view, 1> cmd{ "GET" };
    commands::dispatch(context, cmd);

    REQUIRE(reply.str() == "-ERR wrong number of arguments\r\n");
}

TEST_CASE("dispatch validates argument count for SET", "[commands]")
{
    DBShard shard{ std::pmr::new_delete_resource() };
    Reply reply;
    Context context{ shard, reply };

    std::array<std::string_view, 2> cmd{ "SET", "only_key" };
    commands::dispatch(context, cmd);

    REQUIRE(reply.str() == "-ERR wrong number of arguments\r\n");
}

TEST_CASE("dispatch appends output on repeated calls", "[commands]")
{
    DBShard shard{ std::pmr::new_delete_resource() };
    Reply reply;
    Context context{ shard, reply };

    std::array<std::string_view, 3> set_cmd{ "SET", "k", "v" };
    std::array<std::string_view, 2> bad_set_cmd{ "SET", "k" };

    commands::dispatch(context, set_cmd);
    commands::dispatch(context, bad_set_cmd);

    REQUIRE(reply.str() == "+OK\r\n-ERR wrong number of arguments\r\n");
}
