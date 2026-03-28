#ifndef SPONGE_REDIS_COMMAND_CONTEXT_H
#define SPONGE_REDIS_COMMAND_CONTEXT_H

#include <sponge/redis/application_context.h>
#include <sponge/redis/db_shard.h>

#include "reply.h"

namespace spg::redis {

struct CommandContext {
    ApplicationContext& application_context;
    Reply& reply;

    auto io_context(std::size_t hash) -> boost::asio::io_context&
    {
        return application_context.io_context(hash, by_hash);
    }

    auto shard(std::size_t hash) -> DBShard&
    {
        return application_context.shard(hash, by_hash);
    }
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_CONTEXT_H
