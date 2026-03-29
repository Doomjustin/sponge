#include "command.h"

#include <cstdint>
#include <mutex>

#include <boost/asio.hpp>

#include <sponge/hash.h>
#include <sponge/redis/db_shard.h>
#include <sponge/utility.h>

#include "reply.h"

namespace asio = boost::asio;

namespace spg::redis {

// -------- key commands --------

void command::exists(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.empty()) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'EXISTS' command" });
        return;
    }

    int count = 0;
    for (const auto& key : args) {
        auto exists_key = [&] (auto& handler)
        {
            if (handler.exists())
                ++count;
        };

        context.shard(key).modify(key, exists_key);
    }

    context.reply.append(count);
}

void command::del(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.empty()) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'DEL' command" });
        return;
    }

    int count = 0;
    for (const auto& key : args) {
        auto del_key = [&] (auto& handler)
        {
            if (handler.erase())
                ++count;
        };

        context.shard(key).modify(key, del_key);
    }

    context.reply.append(count);
}

void command::expire(CommandContext& context, std::string_view key, int64_t seconds)
{
    auto expire_key = [&] (auto& handler)
    {
        if (handler.expire(seconds * 1000))
            context.reply.append(ok);
        else
            context.reply.append(-2); // -2 表示不存在
    };
    
    context.shard(key).modify(key, expire_key);
}

void command::persist(CommandContext& context, std::string_view key)
{
    auto persist_key = [&] (auto& handler)
    {
        if (handler.persist())
            context.reply.append(ok);
        else
            context.reply.append(-2); // -2 表示不存在
    };

    context.shard(key).modify(key, persist_key);
}

void command::ttl(CommandContext& context, std::string_view key)
{
    auto ttl_key = [&] (auto& handler)
    {
        // TODO: 实现的耦合有点高，后续有空调整
        if (auto ttl = handler.ttl())
            context.reply.append(*ttl);
        else
            context.reply.append(-2); // -2 表示不存在
    };

    context.shard(key).modify(key, ttl_key);
}

void command::type(CommandContext& context, std::string_view key)
{
    auto type_key = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(BulkString{ "none" });
            return;
        }

        if (handler.get_if(as_type<DBShard::String>))
            context.reply.append(BulkString{ "string" });
        else if (handler.get_if(as_type<DBShard::Integral>))
            context.reply.append(BulkString{ "integer" });
        else if (handler.get_if(as_type<DBShard::ZSet>))
            context.reply.append(BulkString{ "zset" });
        else
            context.reply.append(BulkString{ "unknown" });
    };

    context.shard(key).modify(key, type_key);
}

void command::rename(CommandContext& context, std::string_view old_key, std::string_view new_key)
{
    if (old_key == new_key) {
        context.reply.append(ok);
        return;
    }

    auto old_index = context.index(old_key);
    auto new_index = context.index(new_key);

    if (old_index == new_index) {
        auto rename_key = [&] (auto& handler)
        {
            if (handler.rename(new_key))
                context.reply.append(ok);
            else
                context.reply.append(-2); // -2 表示不存在
        };

        context.shard(old_index).modify(old_key, rename_key);
        return;
    }

    // 跨 shard 重命名，先访问旧 key 获取数据，再修改新 key
    auto& old_shard = context.shard(old_index);
    auto& new_shard = context.shard(new_index); 
    std::scoped_lock locker{ old_shard.mutex_of(old_key), new_shard.mutex_of(new_key) };

    auto old_entry = old_shard.get_entry(old_key);
    if (!old_entry) {
        context.reply.append(-2); // -2 表示不存在
        return;
    }

    new_shard.set(new_key, std::move(*old_entry));
    old_shard.erase(old_key);
    context.reply.append(ok);
}

// -------- string commands --------

// SET key value [EX seconds|PX milliseconds] [NX|XX]
void command::set(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 2) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'SET' command" });
        return;
    }

    std::string_view key = args[0];
    std::string_view value = args[1];

    bool update_not_exists = false;
    bool update_exists = false;
    std::optional<int64_t> expire_ms;

    static constexpr std::string_view expire_in_seconds = "EX";
    static constexpr std::string_view expire_in_milliseconds = "PX";
    static constexpr std::string_view not_exists = "NX";
    static constexpr std::string_view exists = "XX";

    for (size_t i = 2; i < args.size(); ++i) {
        auto option = to_uppercase(args[i]);
        if (option == expire_in_seconds || option == expire_in_milliseconds) {
            if (i + 1 >= args.size()) {
                context.reply.append(Error{ fmt::format("ERR syntax error: expected argument after '{}'", option) });
                return;
            }

            auto time_opt = numeric_cast<int64_t>(args[i + 1]);
            if (!time_opt) {
                context.reply.append(Error{ fmt::format("ERR invalid expire time '{}'", args[i + 1]) });
                return;
            }

            expire_ms = (option == expire_in_seconds) ? (*time_opt * 1000) : *time_opt;
            ++i; // 跳过时间参数
        }
        else if (option == not_exists) {
            update_not_exists = true;
        }
        else if (option == exists) {
            update_exists = true;
        }
        else {
            context.reply.append(Error{ fmt::format("ERR syntax error: unknown option '{}'", option) });
            return;
        }
    }

    if (update_not_exists && update_exists) {
        context.reply.append(Error{ "ERR syntax error: NX and XX options are mutually exclusive" });
        return;
    }

    auto set_string = [&] (auto& handler)
    {
        if (update_exists && !handler.exists()) {
            context.reply.append(null_string);
            return;
        }

        if (update_not_exists && handler.exists()) {
            context.reply.append(null_string);
            return;
        }
        
        handler.emplace(as_type<DBShard::String>, value);

        // 如果没有设置过期的话，set默认会把key设置为永不过期
        if (expire_ms)
            handler.expire(*expire_ms);
        else
            handler.persist();

        context.reply.append(ok);
    };

    context.shard(key).modify(key, set_string);
}

void command::get(CommandContext& context, std::string_view key)
{
    auto get_string = [&] (auto& handler)
    {   
        if (auto* str = handler.get_if(as_type<DBShard::String>))
            context.reply.append(BulkString{ *str });
        else
            context.reply.append(null_string);
    };
   
    context.shard(key).modify(key, get_string);
}

void command::strlen(CommandContext& context, std::string_view key)
{
    auto strlen_string = [&] (auto& handler)
    {
        if (auto* str = handler.get_if(as_type<DBShard::String>))
            context.reply.append(str->size());
        else
            context.reply.append(0);
    };

    context.shard(key).modify(key, strlen_string);
}

// -------- sorted set commands --------

void command::zadd(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 3 || args.size() % 2 == 0) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'ZADD' command" });
        return;
    }

    std::string_view key = args[0];

    std::vector<std::pair<double, std::string_view>> elements;
    elements.reserve((args.size() - 1) / 2);

    // 从 args 中解析出 score 和 member，score 是 double 类型，member 是 string_view
    for (size_t i = 1; i < args.size(); i += 2) {
        auto score_opt = numeric_cast<double>(args[i]);
        if (!score_opt) {
            context.reply.append(Error{ fmt::format("ERR invalid score '{}'", args[i]) });
            return;
        }

        elements.emplace_back(*score_opt, args[i + 1]);
    }

    auto add_to_zset = [&] (auto& handler)
    {
        auto* zset = handler.exists() ?
                     handler.get_if(as_type<DBShard::ZSet>) :
                     handler.emplace(as_type<DBShard::ZSet>);
        
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        int count = 0;
        for (const auto& [score, member] : elements) {
            zset->add(score, member);
            ++count;
        }

        context.reply.append(count);
    };

    context.shard(key).modify(key, add_to_zset);
}

void command::zscore(CommandContext& context, std::string_view key, std::string_view member)
{
    auto zscore_key = [&] (auto& handler)
    {
        auto* zset = handler.get_if(as_type<DBShard::ZSet>);
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        if (auto score = zset->score(member))
            context.reply.append(BulkString{ fmt::format("{}", *score) });
        else
            context.reply.append(null_string);
    };

    context.shard(key).modify(key, zscore_key);
}

void command::bg_rewrite_aof(CommandContext& context)
{
    context.reply.append(ok);
    context.aof().background_rewrite(context.shards());
}

void command::flushall(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() > 1) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'FLUSHALL' command" });
        return;
    }

    auto clear_task = [&context] () -> void
    {
        for (auto& shard: context.shards())
            shard->clear();

        context.aof().reset();
    };

    if (args.empty()) {
        context.reply.append(ok);
        clear_task();
        return;
    }

    auto option = to_uppercase(args[0]);
    if (option != "ASYNC") {
        context.reply.append(Error{ fmt::format("ERR syntax error: unknown option '{}'", option) });
        return;
    }

    std::thread t{ clear_task };
    t.detach();
    context.reply.append(ok);
    return;
}

void command::dbsize(CommandContext& context)
{
    size_t total_size = 0;
    for (const auto& shard : context.shards())
        total_size += shard->size();

    context.reply.append(total_size);
}

} // namespace spg::redis
