#ifndef SPONGE_REDIS_COMMANDS_H
#define SPONGE_REDIS_COMMANDS_H

#include <span>
#include <string_view>

#include "db_shard.h"
#include "reply.h"

namespace spg::redis {

struct Context {
    DBShard& db_shard;
    Reply& reply;
};

struct commands {
    commands() = delete;

    static void dispatch(Context& context, std::span<const std::string_view> cmd);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMANDS_H
