#include "command.h"

#include <variant>

#include <boost/asio.hpp>

#include <sponge/hash.h>

#include "redis/reply.h"
#include "sponge/redis/db_shard.h"

namespace asio = boost::asio;

namespace spg::redis {

namespace {

auto hash(std::string_view key) -> size_t
{
    return hash(key, use_fnv_1a);
}

} // namespace

void command::set(CommandContext& context, std::string_view key, std::string_view value)
{
    auto index = hash(key);
    auto& shard = context.shard(index);
    shard.set(key, value);

    context.reply.append(ok);
}

void command::get(CommandContext& context, std::string_view key)
{
    auto index = hash(key);
    auto& shard = context.shard(index);
    
    shard.access(key, [&] (const DBShard::Value* value) {
        if (value && std::holds_alternative<DBShard::String>(*value))
            context.reply.append(BulkString{ std::get<DBShard::String>(*value) });
        else
            context.reply.append(null_string);
    });
}

} // namespace spg::redis
