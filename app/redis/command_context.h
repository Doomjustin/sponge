#ifndef SPONGE_REDIS_COMMAND_CONTEXT_H
#define SPONGE_REDIS_COMMAND_CONTEXT_H

#include <string>

#include <sponge/tag.h>
#include <sponge/utility.h>

#include "application_context.h"
#include "db_shard.h"
#include "reply.h"

namespace spg::redis {


struct CommandContext {
    ApplicationContext& application_context;
    Reply& reply;
    std::pmr::string aof_buffer;
    bool is_aof_loading = false; // 标记当前是否处于 AOF 加载阶段，避免在加载时写入 AOF

    auto io_context(std::size_t hash) -> boost::asio::io_context&
    {
        return application_context.io_context(hash);
    }

    auto io_context(std::string_view key) -> boost::asio::io_context&
    {
        return application_context.io_context(hashed_key(index(key)));
    }

    auto shard(std::size_t hash) -> DBShard&
    {
        return application_context.shard(hash);
    }

    auto shard(std::string_view key) -> DBShard&
    {
        return application_context.shard(hashed_key(index(key)));
    }

    auto shards() const -> std::span<const std::unique_ptr<DBShard>>
    {
        return application_context.shards();
    }

    auto index(std::string_view key) -> std::size_t
    {
        auto hash = hash_fnv_1a(key);
        return hash % application_context.size();
    }

    void append(std::string_view command)
    {
        aof_buffer.append(command);
    }

    auto aof() -> AOF&
    {
        return application_context.aof();
    }

private:
    static auto hash_fnv_1a(std::string_view key) -> std::size_t
    {
        return hash(use_fnv_1a(key));
    }
};

} // namespace spg::redis

#endif // SPONGE_REDIS_COMMAND_CONTEXT_H
