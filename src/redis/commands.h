#ifndef SPONGE_REDIS_COMMANDS_H
#define SPONGE_REDIS_COMMANDS_H

#include <span>
#include <string_view>

#include "command_context.h"

namespace spg::redis {

struct commands {
    commands() = delete;

    static void dispatch(CommandContext& context, std::span<const std::string_view> cmd);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMANDS_H
