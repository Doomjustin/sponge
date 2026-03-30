#include "command.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include <sponge/hash.h>
#include <sponge/redis/db_shard.h>
#include <sponge/utility.h>

#include "reply.h"

namespace asio = boost::asio;

namespace spg::redis {

namespace {

struct ZAddOptions {
    bool nx = false;
    bool xx = false;
    bool ch = false;
    bool incr = false;
};

auto value_to_string(const DBShard::EntryHandler& handler) -> std::optional<std::string>
{
    if (auto* str = handler.get_if(as_type<DBShard::String>))
        return std::string{ *str };

    if (auto* integer = handler.get_if(as_type<DBShard::Integral>))
        return fmt::format("{}", *integer);

    return std::nullopt;
}

auto value_to_int64(const DBShard::EntryHandler& handler) -> std::optional<int64_t>
{
    if (auto* integer = handler.get_if(as_type<DBShard::Integral>))
        return *integer;

    if (auto* str = handler.get_if(as_type<DBShard::String>)) {
        auto parsed = numeric_cast<int64_t>(*str);
        if (!parsed)
            return std::nullopt;

        return *parsed;
    }

    return std::nullopt;
}

auto normalize_list_index(int64_t index, size_t size) -> int64_t
{
    if (index < 0)
        index += static_cast<int64_t>(size);

    if (index < 0)
        return 0;

    auto max_index = static_cast<int64_t>(size) - 1;
    if (index > max_index)
        return max_index;

    return index;
}

template<typename Popper>
void pop_list(CommandContext& context,
              std::span<const std::string_view> args,
              std::string_view command,
              Popper&& popper)
{
    if (args.empty() || args.size() > 2) {
        context.reply.append(Error{ fmt::format("ERR wrong number of arguments for '{}' command", command) });
        return;
    }

    auto key = args[0];
    std::optional<size_t> count;
    if (args.size() == 2) {
        auto count_opt = numeric_cast<int64_t>(args[1]);
        if (!count_opt || *count_opt <= 0) {
            context.reply.append(Error{ fmt::format("ERR invalid count '{}'", args[1]) });
            return;
        }

        count = static_cast<size_t>(*count_opt);
    }

    auto popper_fn = [&] (auto& handler)
    {
        if (!handler.exists()) {
            if (count)
                context.reply.append(null_array);
            else
                context.reply.append(null_string);
            return;
        }

        auto* list = handler.get_if(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        if (!count) {
            if (list->empty()) {
                context.reply.append(null_string);
                return;
            }

            auto value = popper(*list);
            if (list->empty())
                handler.erase();

            context.reply.append(BulkString{ value });
            return;
        }

        auto actual = std::min(*count, list->size());
        if (actual == 0) {
            context.reply.append(null_array);
            return;
        }

        context.reply.append(ArrayHeader{ actual });
        for (size_t i = 0; i < actual; ++i)
            context.reply.append(BulkString{ popper(*list) });

        if (list->empty())
            handler.erase();
    };

    context.shard(key).modify(key, popper_fn);
}

} // namespace

// -------- key commands --------

void command::exists(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.empty()) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'EXISTS' command" });
        return;
    }

    int count = 0;
    auto exitsxor = [&] (auto& handler)
    {
        if (handler.exists()) ++count;
    };

    for (const auto& key : args)
        context.shard(key).modify(key, exitsxor);

    context.reply.append(count);
}

void command::del(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.empty()) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'DEL' command" });
        return;
    }

    int count = 0;
    auto deleter = [&] (auto& handler)
    {
        if (handler.erase()) ++count;
    };

    for (const auto& key : args)
        context.shard(key).modify(key, deleter);

    context.reply.append(count);
}

void command::expire(CommandContext& context, std::string_view key, int64_t seconds)
{
    auto expiry = [&] (auto& handler)
    {
        if (handler.expire(seconds * 1000))
            context.reply.append(ok);
        else
            context.reply.append(-2); // -2 表示不存在
    };
    
    context.shard(key).modify(key, expiry);
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
        else if (handler.get_if(as_type<DBShard::HashTable>))
            context.reply.append(BulkString{ "hash" });
        else if (handler.get_if(as_type<DBShard::List>))
            context.reply.append(BulkString{ "list" });
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
        
        handler.upsert(as_type<DBShard::String>, value);

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
        auto value = value_to_string(handler);
        if (!value) {
            context.reply.append(null_string);
            return;
        }

        context.reply.append(BulkString{ *value });
    };
   
    context.shard(key).modify(key, get_string);
}

void command::mset(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 2 || args.size() % 2 != 0) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'MSET' command" });
        return;
    }

    for (size_t i = 0; i < args.size(); i += 2) {
        auto key = args[i];
        auto value = args[i + 1];
        auto setter = [&] (auto& handler)
        {
            handler.upsert(as_type<DBShard::String>, value);
            handler.persist();
        };

        context.shard(key).modify(key, setter);
    }

    context.reply.append(ok);
}

void command::mget(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.empty()) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'MGET' command" });
        return;
    }

    context.reply.append(ArrayHeader{ args.size() });
    for (const auto& key : args) {
        auto getter = [&] (auto& handler)
        {
            auto value = value_to_string(handler);
            if (!value) {
                context.reply.append(null_string);
                return;
            }

            context.reply.append(BulkString{ *value });
        };

        context.shard(key).modify(key, getter);
    }
}

void command::incr(CommandContext& context, std::string_view key)
{
    incrby(context, key, 1);
}

void command::decr(CommandContext& context, std::string_view key)
{
    decrby(context, key, 1);
}

void command::incrby(CommandContext& context, std::string_view key, int64_t increment)
{
    auto increaser = [&] (auto& handler)
    {
        int64_t value = 0;
        if (handler.exists()) {
            auto parsed = value_to_int64(handler);
            if (!parsed) {
                context.reply.append(Error{ "ERR value is not an integer or out of range" });
                return;
            }

            value = *parsed;
        }

        if ((increment > 0 && value > std::numeric_limits<int64_t>::max() - increment) ||
            (increment < 0 && value < std::numeric_limits<int64_t>::min() - increment)) {
            context.reply.append(Error{ "ERR increment or decrement would overflow" });
            return;
        }

        value += increment;
        handler.upsert(as_type<DBShard::Integral>, value);
        handler.persist();
        context.reply.append(value);
    };

    context.shard(key).modify(key, increaser);
}

void command::decrby(CommandContext& context, std::string_view key, int64_t decrement)
{
    if (decrement == std::numeric_limits<int64_t>::min()) {
        context.reply.append(Error{ "ERR decrement would overflow" });
        return;
    }

    incrby(context, key, -decrement);
}

void command::append(CommandContext& context, std::string_view key, std::string_view value)
{
    auto appender = [&] (auto& handler)
    {
        std::string base;
        if (handler.exists()) {
            auto current = value_to_string(handler);
            if (!current) {
                context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a string", key) });
                return;
            }

            base = std::move(*current);
        }

        base.append(value);
        handler.upsert(as_type<DBShard::String>, base);
        handler.persist();
        context.reply.append(base.size());
    };

    context.shard(key).modify(key, appender);
}

void command::strlen(CommandContext& context, std::string_view key)
{
    auto strlen_string = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        if (auto* str = handler.get_if(as_type<DBShard::String>))
            context.reply.append(str->size());
        else
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a string", key) });
    };

    context.shard(key).modify(key, strlen_string);
}

// -------- sorted set commands --------

void command::zadd(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 3) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'ZADD' command" });
        return;
    }

    std::string_view key = args[0];
    size_t index = 1;

    ZAddOptions options;
    for (; index < args.size(); ++index) {
        auto option = to_uppercase(args[index]);
        if (option == "NX") {
            options.nx = true;
            continue;
        }

        if (option == "XX") {
            options.xx = true;
            continue;
        }

        if (option == "CH") {
            options.ch = true;
            continue;
        }

        if (option == "INCR") {
            options.incr = true;
            continue;
        }

        break;
    }

    if (options.nx && options.xx) {
        context.reply.append(Error{ "ERR XX and NX options at the same time are not compatible" });
        return;
    }

    auto remain = args.size() - index;
    if (remain == 0 || remain % 2 != 0) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'ZADD' command" });
        return;
    }

    if (options.incr && remain != 2) {
        context.reply.append(Error{ "ERR INCR option supports a single increment-element pair" });
        return;
    }

    std::vector<std::pair<double, std::string_view>> elements;
    elements.reserve(remain / 2);

    for (size_t i = index; i < args.size(); i += 2) {
        auto score_opt = numeric_cast<double>(args[i]);
        if (!score_opt) {
            context.reply.append(Error{ fmt::format("ERR invalid score '{}'", args[i]) });
            return;
        }

        elements.emplace_back(*score_opt, args[i + 1]);
    }

    auto adder = [&] (auto& handler)
    {
        if (handler.exists() && !handler.get_if(as_type<DBShard::ZSet>)) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        if (options.incr) {
            const auto& [increment, member] = elements.front();

            auto* existing = handler.get_if(as_type<DBShard::ZSet>);
            auto old_score = existing ? existing->score(member) : std::optional<double>{};
            if (options.nx && old_score.has_value()) {
                context.reply.append(null_string);
                return;
            }

            if (options.xx && !old_score.has_value()) {
                context.reply.append(null_string);
                return;
            }

            auto* zset = existing ? existing : handler.emplace(as_type<DBShard::ZSet>);
            auto new_score = old_score.value_or(0.0) + increment;
            zset->add(new_score, member);
            context.reply.append(BulkString{ fmt::format("{}", new_score) });
            return;
        }

        size_t added = 0;
        size_t changed = 0;
        for (const auto& [score, member] : elements) {
            auto* existing = handler.get_if(as_type<DBShard::ZSet>);
            auto old_score = existing ? existing->score(member) : std::optional<double>{};

            if (options.nx && old_score.has_value())
                continue;

            if (options.xx && !old_score.has_value())
                continue;

            auto* zset = existing ? existing : handler.emplace(as_type<DBShard::ZSet>);
            zset->add(score, member);

            if (!old_score.has_value())
                ++added;

            if (!old_score.has_value() || *old_score != score)
                ++changed;
        }

        context.reply.append(options.ch ? changed : added);
    };

    context.shard(key).modify(key, adder);
}

void command::zscore(CommandContext& context, std::string_view key, std::string_view member)
{
    auto getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(null_string);
            return;
        }

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

    context.shard(key).modify(key, getter);
}

void command::zcard(CommandContext& context, std::string_view key)
{
    auto cardinality = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        auto* zset = handler.get_if(as_type<DBShard::ZSet>);
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        context.reply.append(zset->size());
    };

    context.shard(key).modify(key, cardinality);
}

void command::zrem(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 2) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'ZREM' command" });
        return;
    }

    auto key = args[0];
    auto remover = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        auto* zset = handler.get_if(as_type<DBShard::ZSet>);
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        size_t removed = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (zset->erase(args[i]))
                ++removed;
        }

        if (zset->size() == 0)
            handler.erase();

        context.reply.append(removed);
    };

    context.shard(key).modify(key, remover);
}

void command::zrange(CommandContext& context, std::string_view key, int64_t start, int64_t stop)
{
    auto ranger = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto* zset = handler.get_if(as_type<DBShard::ZSet>);
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        auto size = zset->size();
        if (size == 0) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto from = normalize_list_index(start, size);
        auto to = normalize_list_index(stop, size);
        if (from > to) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto count = static_cast<size_t>(to - from + 1);
        context.reply.append(ArrayHeader{ count });

        auto it = zset->begin();
        for (int64_t i = 0; i < from; ++i)
            ++it;

        for (size_t i = 0; i < count; ++i, ++it) {
            auto [member, score] = *it;
            context.reply.append(BulkString{ member });
        }
    };

    context.shard(key).modify(key, ranger);
}

void command::zcount(CommandContext& context, std::string_view key, double min, double max)
{
    auto counter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        auto* zset = handler.get_if(as_type<DBShard::ZSet>);
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        context.reply.append(zset->zcount(min, max));
    };

    context.shard(key).modify(key, counter);
}

void command::zrank(CommandContext& context, std::string_view key, std::string_view member)
{
    auto ranker = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(null_string);
            return;
        }

        auto* zset = handler.get_if(as_type<DBShard::ZSet>);
        if (!zset) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a sorted set", key) });
            return;
        }

        if (auto rank = zset->zrank(member))
            context.reply.append(*rank);
        else
            context.reply.append(null_string);
    };

    context.shard(key).modify(key, ranker);
}

// -------- list commands --------

void command::lpush(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 2) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'LPUSH' command" });
        return;
    }

    auto key = args[0];
    auto pusher = [&] (auto& handler)
    {
        auto* list = handler.exists() ? handler.get_if(as_type<DBShard::List>) : handler.emplace(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        for (size_t i = 1; i < args.size(); ++i)
            list->push_front(DBShard::String{ args[i] });

        context.reply.append(list->size());
    };

    context.shard(key).modify(key, pusher);
}

void command::rpush(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 2) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'RPUSH' command" });
        return;
    }

    auto key = args[0];
    auto pusher = [&] (auto& handler)
    {
        auto* list = handler.exists() ? handler.get_if(as_type<DBShard::List>) : handler.emplace(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        for (size_t i = 1; i < args.size(); ++i)
            list->push_back(DBShard::String{ args[i] });

        context.reply.append(list->size());
    };

    context.shard(key).modify(key, pusher);
}

void command::lpop(CommandContext& context, std::span<const std::string_view> args)
{
    pop_list(context, args, "LPOP", [] (auto& list) {
        DBShard::String value = std::move(list.front());
        list.pop_front();
        return value;
    });
}

void command::rpop(CommandContext& context, std::span<const std::string_view> args)
{
    pop_list(context, args, "RPOP", [] (auto& list) {
        DBShard::String value = std::move(list.back());
        list.pop_back();
        return value;
    });
}

void command::lrange(CommandContext& context, std::string_view key, int64_t start, int64_t stop)
{
    auto ranger = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto* list = handler.get_if(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        if (list->empty()) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto size = list->size();
        auto from = normalize_list_index(start, size);
        auto to = normalize_list_index(stop, size);
        if (from > to) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto count = static_cast<size_t>(to - from + 1);
        context.reply.append(ArrayHeader{ count });

        auto it = list->begin();
        std::advance(it, from);
        for (size_t i = 0; i < count; ++i, ++it)
            context.reply.append(BulkString{ *it });
    };

    context.shard(key).modify(key, ranger);
}

void command::llen(CommandContext& context, std::string_view key)
{
    auto len_getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        auto* list = handler.get_if(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        context.reply.append(list->size());
    };

    context.shard(key).modify(key, len_getter);
}

void command::lindex(CommandContext& context, std::string_view key, int64_t index)
{
    auto getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(null_string);
            return;
        }

        auto* list = handler.get_if(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        auto size = static_cast<int64_t>(list->size());
        if (index < 0)
            index += size;

        if (index < 0 || index >= size) {
            context.reply.append(null_string);
            return;
        }

        auto it = list->begin();
        std::advance(it, index);
        context.reply.append(BulkString{ *it });
    };

    context.shard(key).modify(key, getter);
}

void command::lset(CommandContext& context, std::string_view key, int64_t index, std::string_view value)
{
    auto setter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(Error{ "ERR no such key" });
            return;
        }

        auto* list = handler.get_if(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        auto size = static_cast<int64_t>(list->size());
        if (index < 0)
            index += size;

        if (index < 0 || index >= size) {
            context.reply.append(Error{ "ERR index out of range" });
            return;
        }

        auto it = list->begin();
        std::advance(it, index);
        *it = DBShard::String{ value };
        context.reply.append(ok);
    };

    context.shard(key).modify(key, setter);
}

void command::ltrim(CommandContext& context, std::string_view key, int64_t start, int64_t stop)
{
    auto trimmer = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(ok);
            return;
        }

        auto* list = handler.get_if(as_type<DBShard::List>);
        if (!list) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a list", key) });
            return;
        }

        auto size = list->size();
        auto from = normalize_list_index(start, size);
        auto to = normalize_list_index(stop, size);

        if (from > to) {
            handler.erase();
            context.reply.append(ok);
            return;
        }

        auto back_remove = static_cast<int64_t>(size) - 1 - to;
        for (int64_t i = 0; i < back_remove; ++i)
            list->pop_back();

        for (int64_t i = 0; i < from; ++i)
            list->pop_front();

        if (list->empty())
            handler.erase();

        context.reply.append(ok);
    };

    context.shard(key).modify(key, trimmer);
}

// -------- hash commands --------

void command::hset(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 3 || args.size() % 2 == 0) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'HSET' command" });
        return;
    }

    auto key = args[0];

    auto setter = [&] (auto& handler)
    {
        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (handler.exists() && !hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;            
        }

        if (!hash_table)
            hash_table = handler.emplace(as_type<DBShard::HashTable>);

        int count = 0;
        for (size_t i = 1; i < args.size(); i += 2) {
            auto field = args[i];
            auto value = args[i + 1];
            if (!hash_table->contains(field))
                ++count;

            hash_table->insert_or_assign(field, value);
        }

        context.reply.append(count);
    };

    context.shard(key).modify(key, setter);
}

void command::hget(CommandContext& context, std::string_view key, std::string_view field)
{
    auto getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(null_string);
            return;
        }

        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        auto iter = hash_table->find(field);
        if (iter != hash_table->end())
            context.reply.append(BulkString{ iter->second });
        else
            context.reply.append(null_string);
    };

    context.shard(key).modify(key, getter);
}

void command::hmget(CommandContext& context, std::span<const std::string_view> args)
{
    auto getter = [&] (auto& handler)
    {
        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", args[0]) });
            return;
        }

        context.reply.append(ArrayHeader{ args.size() - 1 });
        for (size_t i = 1; i < args.size(); ++i) {
            auto field = args[i];
            auto iter = hash_table->find(field);
            if (iter != hash_table->end())
                context.reply.append(BulkString{ iter->second });
            else
                context.reply.append(null_string);
        }
    };

    context.shard(args[0]).modify(args[0], getter);
}

void command::hincrby(CommandContext& context, std::string_view key, std::string_view field, int64_t increment)
{
    auto increaser = [&] (auto& handler)
    {
        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        auto iter = hash_table->find(field);
        int64_t value = 0;
        if (iter != hash_table->end()) {
            auto value_opt = numeric_cast<int64_t>(iter->second);
            if (!value_opt) {
                context.reply.append(Error{ fmt::format("ERR hash value for field '{}' is not an integer", field) });
                return;
            }

            value = *value_opt;
        }

        if (value > std::numeric_limits<int64_t>::max() - increment) {
            context.reply.append(Error{ "ERR increment would cause overflow" });
            return;
        }

        value += increment;
        (*hash_table)[field] = value;
        context.reply.append(value);
    };

    context.shard(key).modify(key, increaser);
}

void command::hdel(CommandContext& context, std::span<const std::string_view> args)
{
    if (args.size() < 2) {
        context.reply.append(Error{ "ERR wrong number of arguments for 'HDEL' command" });
        return;
    }

    auto delete_hash_table = [&] (auto& handler)
    {
        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", args[0]) });
            return;
        }

        int count = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            auto field = args[i];
            if (hash_table->erase(field) > 0)
                ++count;
        }

        context.reply.append(count);
    };

    context.shard(args[0]).modify(args[0], delete_hash_table);
}

void command::hlen(CommandContext& context, std::string_view key)
{
    auto len_getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        context.reply.append(hash_table->size());
    };

    context.shard(key).modify(key, len_getter);
}

void command::hgetall(CommandContext& context, std::string_view key)
{
    auto getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        auto size = hash_table->size();
        context.reply.append(ArrayHeader{ size * 2 }); // 每个键值对占
        for (const auto& [field, value] : *hash_table) {
            context.reply.append(BulkString{ field });
            context.reply.append(BulkString{ value });
        }
    };

    context.shard(key).modify(key, getter);
}

void command::hkeys(CommandContext& context, std::string_view key)
{
    auto getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        context.reply.append(ArrayHeader{ hash_table->size() });
        for (const auto& [field, value] : *hash_table)
            context.reply.append(BulkString{ field });
    };

    context.shard(key).modify(key, getter);
}

void command::hvals(CommandContext& context, std::string_view key)
{
    auto getter = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(ArrayHeader{ 0 });
            return;
        }

        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        context.reply.append(ArrayHeader{ hash_table->size() });
        for (const auto& [field, value] : *hash_table)
            context.reply.append(BulkString{ value });
    };

    context.shard(key).modify(key, getter);
}

void command::hexists(CommandContext& context, std::string_view key, std::string_view field)
{
    auto checker = [&] (auto& handler)
    {
        if (!handler.exists()) {
            context.reply.append(0);
            return;
        }

        auto* hash_table = handler.get_if(as_type<DBShard::HashTable>);
        if (!hash_table) {
            context.reply.append(Error{ fmt::format("ERR key '{}' does not hold a hash", key) });
            return;
        }

        context.reply.append(hash_table->contains(field) ? 1 : 0);
    };

    context.shard(key).modify(key, checker);
}

// ------- other commands --------

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
