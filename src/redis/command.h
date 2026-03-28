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

    static void strlen(CommandContext& context, std::string_view key);

    // -------- sorted set commands --------
    
    static void zadd(CommandContext& context, std::span<const std::string_view> args);

    static void zscore(CommandContext& context, std::string_view key, std::string_view member);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_H
