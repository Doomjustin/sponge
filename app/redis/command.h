#ifndef SPONGE_REDIS_COMMAND_H
#define SPONGE_REDIS_COMMAND_H

#include <string_view>

#include <boost/asio.hpp>

#include "command_context.h"

namespace spg::redis {

struct command {
    command() = delete;

    // -------- key commands --------

    static void exists(CommandContext& context, std::span<const std::string_view> args);

    static void del(CommandContext& context, std::span<const std::string_view> args);

    static void expire(CommandContext& context, std::string_view key, int64_t seconds);

    static void persist(CommandContext& context, std::string_view key);

    static void ttl(CommandContext& context, std::string_view key);

    static void type(CommandContext& context, std::string_view key);

    static void rename(CommandContext& context, std::string_view old_key, std::string_view new_key);

    // -------- string commands --------
    
    static void set(CommandContext& context, std::span<const std::string_view> args);

    static void get(CommandContext& context, std::string_view key);

    static void mset(CommandContext& context, std::span<const std::string_view> args);

    static void mget(CommandContext& context, std::span<const std::string_view> args);

    static void incr(CommandContext& context, std::string_view key);

    static void decr(CommandContext& context, std::string_view key);

    static void incrby(CommandContext& context, std::string_view key, int64_t increment);

    static void decrby(CommandContext& context, std::string_view key, int64_t decrement);

    static void append(CommandContext& context, std::string_view key, std::string_view value);

    static void strlen(CommandContext& context, std::string_view key);

    // -------- sorted set commands --------
    
    static void zadd(CommandContext& context, std::span<const std::string_view> args);

    static void zscore(CommandContext& context, std::string_view key, std::string_view member);

    static void zcard(CommandContext& context, std::string_view key);

    static void zrem(CommandContext& context, std::span<const std::string_view> args);

    static void zrange(CommandContext& context, std::string_view key, int64_t start, int64_t stop);

    static void zcount(CommandContext& context, std::string_view key, double min, double max);

    static void zrank(CommandContext& context, std::string_view key, std::string_view member);

    // -------- list commands --------

    static void lpush(CommandContext& context, std::span<const std::string_view> args);

    static void rpush(CommandContext& context, std::span<const std::string_view> args);

    static void lpop(CommandContext& context, std::span<const std::string_view> args);

    static void rpop(CommandContext& context, std::span<const std::string_view> args);

    static void lrange(CommandContext& context, std::string_view key, int64_t start, int64_t stop);

    static void llen(CommandContext& context, std::string_view key);

    static void lindex(CommandContext& context, std::string_view key, int64_t index);

    static void lset(CommandContext& context, std::string_view key, int64_t index, std::string_view value);

    static void ltrim(CommandContext& context, std::string_view key, int64_t start, int64_t stop);

    // -------- hash commands --------

    static void hset(CommandContext& context, std::span<const std::string_view> args);

    static void hget(CommandContext& context, std::string_view key, std::string_view field);

    static void hmget(CommandContext& context, std::span<const std::string_view> args);

    static void hincrby(CommandContext& context, std::string_view key, std::string_view field, int64_t increment);

    static void hdel(CommandContext& context, std::span<const std::string_view> args);

    static void hlen(CommandContext& context, std::string_view key);

    static void hgetall(CommandContext& context, std::string_view key);

    static void hkeys(CommandContext& context, std::string_view key);

    static void hvals(CommandContext& context, std::string_view key);

    static void hexists(CommandContext& context, std::string_view key, std::string_view field);

    // ------- other commands --------

    static void bg_rewrite_aof(CommandContext& context);

    static void flushall(CommandContext& context, std::span<const std::string_view> args);

    static void dbsize(CommandContext& context);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_H
