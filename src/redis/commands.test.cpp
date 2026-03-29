#include "commands.h"

#include <array>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

namespace {

void run_dispatch(ApplicationContext& app,
                  Reply& reply,
                  std::span<const std::string_view> command_args)
{
    resp::Command request{ .arguments = {}, .raw = {} };
    request.arguments.insert(request.arguments.end(), command_args.begin(), command_args.end());

    CommandContext context{ .application_context = app, .reply = reply };
    commands::dispatch(context, request);
}

} // namespace

TEST_CASE("dispatch handles known command case-insensitively", "[commands]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;

    std::array<std::string_view, 3> command_args{ "set", "mykey", "myvalue" };
    run_dispatch(app, reply, command_args);

    REQUIRE(reply.str() == "+OK\r\n");
}

TEST_CASE("dispatch returns error for unknown command", "[commands]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;

    std::array<std::string_view, 1> command_args{ "no_such_cmd" };
    run_dispatch(app, reply, command_args);

    REQUIRE(reply.str() == "-ERR unknown command 'NO_SUCH_CMD'\r\n");
}

TEST_CASE("dispatch validates argument count for GET", "[commands]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;

    std::array<std::string_view, 1> command_args{ "GET" };
    run_dispatch(app, reply, command_args);

    REQUIRE(reply.str() == "-ERR wrong number of arguments\r\n");
}

TEST_CASE("dispatch validates argument count for SET", "[commands]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;

    std::array<std::string_view, 2> command_args{ "SET", "only_key" };
    run_dispatch(app, reply, command_args);

    REQUIRE(reply.str() == "-ERR wrong number of arguments for 'SET' command\r\n");
}

TEST_CASE("dispatch appends output on repeated calls", "[commands]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;

    std::array<std::string_view, 3> set_cmd{ "SET", "k", "v" };
    std::array<std::string_view, 2> bad_set_cmd{ "SET", "k" };

    run_dispatch(app, reply, set_cmd);
    run_dispatch(app, reply, bad_set_cmd);

    REQUIRE(reply.str() == "+OK\r\n-ERR wrong number of arguments for 'SET' command\r\n");
}
