#ifndef SPONGE_REDIS_COMMANDS_H
#define SPONGE_REDIS_COMMANDS_H

#include <boost/asio.hpp>

#include "command_context.h"
#include "protocol.h"

namespace spg::redis {

struct commands {
    commands() = delete;

    static void dispatch(CommandContext& context, const resp::Command& cmd);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMANDS_H
