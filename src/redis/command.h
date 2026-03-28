#ifndef SPONGE_REDIS_COMMAND_H
#define SPONGE_REDIS_COMMAND_H

#include <string_view>

#include "command_context.h"

namespace spg::redis {

struct command {
    command() = delete;

    static void set(CommandContext& context, std::string_view key, std::string_view value);

    static void get(CommandContext& context, std::string_view key);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_H
