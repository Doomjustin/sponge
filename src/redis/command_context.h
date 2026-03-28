#ifndef SPONGE_REDIS_COMMAND_CONTEXT_H
#define SPONGE_REDIS_COMMAND_CONTEXT_H

#include <sponge/redis/db_shard.h>

#include "reply.h"

namespace spg::redis {

struct CommandContext {
    std::span<DBShard> shards;
    Reply& reply;

    auto shard(std::string_view key) noexcept -> DBShard&
    {
        size_t key_hash = hash(key, use_fnv_1a);
        return shard(key_hash, by_hash);
    }
    
    auto shard(size_t key_hash, ByHashT by_hash) noexcept -> DBShard&
    {
        return shards[key_hash % shards.size()];
    }

    auto shard(size_t index) noexcept -> DBShard& { return shards[index]; }
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_CONTEXT_H
