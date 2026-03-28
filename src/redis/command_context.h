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

    auto io_context(std::string_view key) -> boost::asio::io_context&
    {
        auto hash = hash_fnv_1a(key);
        return application_context.io_context(hash, by_hash);
    }

    auto shard(std::size_t hash) -> DBShard&
    {
        return application_context.shard(hash, by_hash);
    }

    auto shard(std::string_view key) -> DBShard&
    {
        auto hash = hash_fnv_1a(key);
        return application_context.shard(hash, by_hash);
    }

    auto resource(std::size_t hash) -> DBShard::MemoryResource*
    {
        return application_context.resource(hash);
    }

    auto resource(std::string_view key) -> DBShard::MemoryResource*
    {
        auto hash = hash_fnv_1a(key);
        return application_context.resource(hash);
    }

private:
    static auto hash_fnv_1a(std::string_view key) -> std::size_t
    {
        return hash(key, use_fnv_1a);
    }
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_CONTEXT_H
