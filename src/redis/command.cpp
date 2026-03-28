#include "command.h"

#include <sponge/hash.h>

namespace spg::redis {

void command::set(CommandContext& context, std::string_view key, std::string_view value)
{
    auto& shard = context.shard(key);
    shard.set(key, value);

    context.reply.append(ok);
}

void command::get(CommandContext& context, std::string_view key)
{
    auto& shard = context.shard(key);
    auto value = shard.get(key, as_string);
    if (value)
        context.reply.append(BulkString{ *value });
    else
        context.reply.append(null_string);
}

} // namespace spg::redis
