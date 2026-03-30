#include "command.h"

#include <array>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

namespace {

auto make_context(ApplicationContext& app, Reply& reply) -> CommandContext
{
    return CommandContext{ .application_context = app, .reply = reply };
}

} // namespace

TEST_CASE("set returns ok", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> args{ "k", "v" };
    command::set(context, args);

    REQUIRE(reply.str() == "+OK\r\n");
}

TEST_CASE("get returns bulk string for existing key", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> set_args{ "k", "v" };
    command::set(context, set_args);
    reply.clear();
    command::get(context, "k");

    REQUIRE(reply.str() == "$1\r\nv\r\n");
}

TEST_CASE("mset returns ok", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 4> args{ "k1", "v1", "k2", "v2" };
    command::mset(context, args);

    REQUIRE(reply.str() == "+OK\r\n");
}

TEST_CASE("mget returns array with existing and missing values", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 4> mset_args{ "k1", "v1", "k2", "v2" };
    std::array<std::string_view, 3> mget_args{ "k1", "missing", "k2" };
    command::mset(context, mset_args);
    reply.clear();
    command::mget(context, mget_args);

    REQUIRE(reply.str() == "*3\r\n$2\r\nv1\r\n$-1\r\n$2\r\nv2\r\n");
}

TEST_CASE("incr initializes missing key to one", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::incr(context, "counter");

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("decrby decreases existing integer value", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> set_args{ "counter", "3" };
    command::set(context, set_args);
    reply.clear();
    command::decrby(context, "counter", 2);

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("append returns new length", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> set_args{ "k", "ab" };
    command::set(context, set_args);
    reply.clear();
    command::append(context, "k", "cd");

    REQUIRE(reply.str() == ":4\r\n");
}

TEST_CASE("hset returns added field count", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> args{ "h", "f", "v" };
    command::hset(context, args);

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("hlen returns field count", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 5> hset_args{ "h", "f1", "v1", "f2", "v2" };
    command::hset(context, hset_args);
    reply.clear();
    command::hlen(context, "h");

    REQUIRE(reply.str() == ":2\r\n");
}

TEST_CASE("lpush returns list length", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> args{ "list", "a", "b" };
    command::lpush(context, args);

    REQUIRE(reply.str() == ":2\r\n");
}

TEST_CASE("lrange returns requested members", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 4> rpush_args{ "list", "a", "b", "c" };
    command::rpush(context, rpush_args);
    reply.clear();
    command::lrange(context, "list", 0, 1);

    REQUIRE(reply.str() == "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
}

TEST_CASE("zadd returns added count", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> args{ "z", "1", "a" };
    command::zadd(context, args);

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("zrange returns members by score order", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 5> zadd_args{ "z", "2", "b", "1", "a" };
    command::zadd(context, zadd_args);
    reply.clear();
    command::zrange(context, "z", 0, -1);

    REQUIRE(reply.str() == "*2\r\n$1\r\na\r\n$1\r\nb\r\n");
}

// -------- error / edge cases for existing commands --------

TEST_CASE("get returns null for missing key", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::get(context, "missing");

    REQUIRE(reply.str() == "$-1\r\n");
}

TEST_CASE("incr returns error for non-integer value", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> set_args{ "k", "not_a_number" };
    command::set(context, set_args);
    reply.clear();
    command::incr(context, "k");

    REQUIRE(reply.str() == "-ERR value is not an integer or out of range\r\n");
}

TEST_CASE("set with nx does not overwrite existing key", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> set_args{ "k", "original" };
    command::set(context, set_args);
    reply.clear();
    std::array<std::string_view, 3> nx_args{ "k", "new", "NX" };
    command::set(context, nx_args);

    REQUIRE(reply.str() == "$-1\r\n");
}

TEST_CASE("lrange on missing key returns empty array", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::lrange(context, "missing", 0, -1);

    REQUIRE(reply.str() == "*0\r\n");
}

TEST_CASE("hget returns error on type mismatch", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> set_args{ "k", "v" };
    command::set(context, set_args);
    reply.clear();
    command::hget(context, "k", "field");

    REQUIRE(reply.str().starts_with("-ERR"));
}

TEST_CASE("llen returns zero for missing key", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::llen(context, "missing");

    REQUIRE(reply.str() == ":0\r\n");
}

// -------- hkeys / hvals / hexists --------

TEST_CASE("hkeys returns field names of hash", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> hset_args{ "h", "f", "v" };
    command::hset(context, hset_args);
    reply.clear();
    command::hkeys(context, "h");

    REQUIRE(reply.str() == "*1\r\n$1\r\nf\r\n");
}

TEST_CASE("hvals returns values of hash", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> hset_args{ "h", "f", "v" };
    command::hset(context, hset_args);
    reply.clear();
    command::hvals(context, "h");

    REQUIRE(reply.str() == "*1\r\n$1\r\nv\r\n");
}

TEST_CASE("hexists returns one for existing field", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> hset_args{ "h", "f", "v" };
    command::hset(context, hset_args);
    reply.clear();
    command::hexists(context, "h", "f");

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("hexists returns zero for missing field", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> hset_args{ "h", "f", "v" };
    command::hset(context, hset_args);
    reply.clear();
    command::hexists(context, "h", "no_such_field");

    REQUIRE(reply.str() == ":0\r\n");
}

// -------- lindex / lset / ltrim --------

TEST_CASE("lindex returns element at positive index", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 4> rpush_args{ "list", "a", "b", "c" };
    command::rpush(context, rpush_args);
    reply.clear();
    command::lindex(context, "list", 1);

    REQUIRE(reply.str() == "$1\r\nb\r\n");
}

TEST_CASE("lindex returns element at negative index", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 4> rpush_args{ "list", "a", "b", "c" };
    command::rpush(context, rpush_args);
    reply.clear();
    command::lindex(context, "list", -1);

    REQUIRE(reply.str() == "$1\r\nc\r\n");
}

TEST_CASE("lindex returns null for out of range index", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> rpush_args{ "list", "a" };
    command::rpush(context, rpush_args);
    reply.clear();
    command::lindex(context, "list", 99);

    REQUIRE(reply.str() == "$-1\r\n");
}

TEST_CASE("lset updates element at index", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> rpush_args{ "list", "a", "b" };
    command::rpush(context, rpush_args);
    command::lset(context, "list", 0, "x");
    reply.clear();
    command::lindex(context, "list", 0);

    REQUIRE(reply.str() == "$1\r\nx\r\n");
}

TEST_CASE("lset returns error for out of range index", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> rpush_args{ "list", "a" };
    command::rpush(context, rpush_args);
    reply.clear();
    command::lset(context, "list", 99, "x");

    REQUIRE(reply.str().starts_with("-ERR"));
}

TEST_CASE("ltrim reduces list to specified range", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 5> rpush_args{ "list", "a", "b", "c", "d" };
    command::rpush(context, rpush_args);
    command::ltrim(context, "list", 1, 2);
    reply.clear();
    command::lrange(context, "list", 0, -1);

    REQUIRE(reply.str() == "*2\r\n$1\r\nb\r\n$1\r\nc\r\n");
}

// -------- zcount / zrank --------

TEST_CASE("zcount counts members within score range", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 7> zadd_args{ "z", "1", "a", "2", "b", "3", "c" };
    command::zadd(context, zadd_args);
    reply.clear();
    command::zcount(context, "z", 1.0, 2.0);

    REQUIRE(reply.str() == ":2\r\n");
}

TEST_CASE("zrank returns zero-based rank of member", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 5> zadd_args{ "z", "1", "a", "2", "b" };
    command::zadd(context, zadd_args);
    reply.clear();
    command::zrank(context, "z", "b");

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("zrank returns null for missing member", "[command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> zadd_args{ "z", "1", "a" };
    command::zadd(context, zadd_args);
    reply.clear();
    command::zrank(context, "z", "missing");

    REQUIRE(reply.str() == "$-1\r\n");
}
