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

TEST_CASE("执行SET命令时应返回OK", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 2> args{ "k", "v" };
    command::set(context, args);

    REQUIRE(reply.str() == "+OK\r\n");
}

TEST_CASE("执行GET命令时应返回存在键的批量字符串", "[redis][command]")
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

TEST_CASE("执行MSET命令时应返回OK", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 4> args{ "k1", "v1", "k2", "v2" };
    command::mset(context, args);

    REQUIRE(reply.str() == "+OK\r\n");
}

TEST_CASE("执行MGET命令时应返回包含存在值与缺失值的数组", "[redis][command]")
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

TEST_CASE("执行INCR命令且键缺失时应初始化为1", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::incr(context, "counter");

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("执行DECRBY命令且键为整数时应减少其值", "[redis][command]")
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

TEST_CASE("执行APPEND命令时应返回新长度", "[redis][command]")
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

TEST_CASE("执行STRLEN命令时应返回0（键缺失）", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::strlen(context, "missing");

    REQUIRE(reply.str() == ":0\r\n");
}

TEST_CASE("执行HSET命令时应返回新增字段数量", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> args{ "h", "f", "v" };
    command::hset(context, args);

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("执行HLEN命令时应返回字段数量", "[redis][command]")
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

TEST_CASE("执行HGET命令时应返回缺失键的空值", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::hget(context, "missing", "field");

    REQUIRE(reply.str() == "$-1\r\n");
}

TEST_CASE("执行HGETALL命令且键缺失时应返回空数组", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::hgetall(context, "missing");

    REQUIRE(reply.str() == "*0\r\n");
}

TEST_CASE("执行HKEYS命令且键缺失时应返回空数组", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::hkeys(context, "missing");

    REQUIRE(reply.str() == "*0\r\n");
}

TEST_CASE("执行HVALS命令且键缺失时应返回空数组", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::hvals(context, "missing");

    REQUIRE(reply.str() == "*0\r\n");
}

TEST_CASE("执行LPUSH命令时应返回列表长度", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> args{ "list", "a", "b" };
    command::lpush(context, args);

    REQUIRE(reply.str() == ":2\r\n");
}

TEST_CASE("执行LRANGE命令时应返回请求的成员", "[redis][command]")
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

TEST_CASE("执行ZADD命令时应返回新增数量", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    std::array<std::string_view, 3> args{ "z", "1", "a" };
    command::zadd(context, args);

    REQUIRE(reply.str() == ":1\r\n");
}

TEST_CASE("执行ZRANGE命令时应返回按分数排序的成员", "[redis][command]")
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

TEST_CASE("执行GET命令时应返回缺失键的空值", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::get(context, "missing");

    REQUIRE(reply.str() == "$-1\r\n");
}

TEST_CASE("执行INCR命令时应返回非整数值错误", "[redis][command]")
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

TEST_CASE("执行SET NX且键已存在时应不覆盖旧值", "[redis][command]")
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

TEST_CASE("执行LRANGE命令且键缺失时应返回空数组", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::lrange(context, "missing", 0, -1);

    REQUIRE(reply.str() == "*0\r\n");
}

TEST_CASE("执行HGET命令时应返回类型不匹配错误", "[redis][command]")
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

TEST_CASE("执行LLEN命令时应返回0（键缺失）", "[redis][command]")
{
    ApplicationContext app{ 1, "/dev/null" };
    Reply reply;
    auto context = make_context(app, reply);

    command::llen(context, "missing");

    REQUIRE(reply.str() == ":0\r\n");
}

// -------- hkeys / hvals / hexists --------

TEST_CASE("执行HKEYS命令时应返回哈希字段名集合", "[redis][command]")
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

TEST_CASE("执行HVALS命令时应返回哈希值集合", "[redis][command]")
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

TEST_CASE("执行HEXISTS命令时应返回1（字段存在）", "[redis][command]")
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

TEST_CASE("执行HEXISTS命令时应返回0（字段缺失）", "[redis][command]")
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

TEST_CASE("执行LINDEX命令时应返回正索引位置元素", "[redis][command]")
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

TEST_CASE("执行LINDEX命令时应返回负索引位置元素", "[redis][command]")
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

TEST_CASE("执行LINDEX命令时应返回越界索引空值", "[redis][command]")
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

TEST_CASE("执行LSET命令更新指定索引时应更新成功", "[redis][command]")
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

TEST_CASE("执行LSET命令时应返回索引越界错误", "[redis][command]")
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

TEST_CASE("执行LTRIM命令时应裁剪到指定范围", "[redis][command]")
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

TEST_CASE("执行ZCOUNT命令时应统计分数范围内成员数", "[redis][command]")
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

TEST_CASE("执行ZRANK命令时应返回成员的零基排名", "[redis][command]")
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

TEST_CASE("执行ZRANK命令且成员缺失时应返回空值", "[redis][command]")
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
