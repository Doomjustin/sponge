#ifndef SPONGE_REDIS_COMMAND_H
#define SPONGE_REDIS_COMMAND_H

#include <string_view>

#include <boost/asio.hpp>

#include "command_context.h"

namespace spg::redis {

struct command {
    command() = delete;

    static void set(CommandContext& context, std::span<const std::string_view> args);

    static void get(CommandContext& context, std::string_view key);

    static void strlen(CommandContext& context, std::string_view key);

    static void expire(CommandContext& context, std::string_view key, int64_t seconds);

    static void ttl(CommandContext& context, std::string_view key);

    static void zadd(CommandContext& context, std::span<const std::string_view> args);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_H
