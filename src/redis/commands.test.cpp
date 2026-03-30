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
