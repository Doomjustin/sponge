#include <sponge/redis/db_shard.h>

#include "sponge/redis/ttl_manager.h"

namespace spg::redis {

DBShard::DBShard(MemoryResource* resource)
  : resource_{ resource }, 
    tables_{ resource }
{
}

auto DBShard::get(std::string_view key, AsStringT t) -> std::optional<std::string_view>
{
    auto get_if_not_expired = [&] (const Entry* entry) -> std::optional<std::string_view> 
    {
        if (!entry)
            return {};

        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return {};
        }

        if (std::holds_alternative<String>(entry->value))
            return std::get<String>(entry->value);

        return {};
    };

    return tables_.access(key, get_if_not_expired);
}

auto DBShard::get(std::string_view key, AsIntegralT t) -> std::optional<Integral>
{
    auto get_if_not_expired = [&] (const Entry* entry) -> std::optional<Integral> 
    {
        if (!entry)
            return {};

        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return {};
        }

        if (std::holds_alternative<Integral>(entry->value))
            return std::get<Integral>(entry->value);

        return {};
    };

    return tables_.access(key, get_if_not_expired);
}

void DBShard::set(std::string_view key, std::string_view value)
{
    Entry entry{ .value = String{ value, resource_ },
                 .expire_at = TTLManager::PERSISTENT_INTEGRAL };
                 
    tables_.set(key, std::move(entry));
}

auto DBShard::erase(std::string_view key) -> bool
{
    auto erase_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> bool 
    {
        if (it == segment.end())
            return false;

        segment.erase(it);
        return true;
    };

    return modify(key, erase_if_not_expired);
}

auto DBShard::holds(std::string_view key, AsStringT t) -> bool
{
    auto holds_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> bool 
    {
        if (it == segment.end())
            return false;

        return std::holds_alternative<String>(it->second.value);
    };

    return modify(key, holds_if_not_expired);
}

auto DBShard::holds(std::string_view key, AsIntegralT t) -> bool
{
    auto holds_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> bool 
    {
        if (it == segment.end())
            return false;

        return std::holds_alternative<Integral>(it->second.value);
    };

    return modify(key, holds_if_not_expired);
}

auto DBShard::exists(std::string_view key) -> bool
{
    auto exists_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> bool 
    {
        if (it == segment.end())
            return false;

        return true;
    };

    return modify(key, exists_if_not_expired);
}

auto DBShard::type_of(std::string_view key) -> ValueType
{
    auto type_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> ValueType
    {
        if (it == segment.end())
            return ValueType::NotExist;

        // 无论是string还是int都返回String类型，保持和redis一致
        if (std::holds_alternative<String>(it->second.value))
            return ValueType::String;

        if (std::holds_alternative<Integral>(it->second.value))
            return ValueType::String;

        return ValueType::NotExist;
    };

    return modify(key, type_if_not_expired);
}

auto DBShard::expire(std::string_view key, MilliSeconds ttl) -> bool
{
    auto expire_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> bool 
    {
        if (it == segment.end())
            return false;

        it->second.expire_at = TTLManager::expire_at(ttl, return_integral);
        return true;
    };

    return modify(key, expire_if_not_expired);
}

auto DBShard::persist(std::string_view key) -> bool
{
    auto persist_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> bool 
    {
        if (it == segment.end())
            return false;

        it->second.expire_at = TTLManager::PERSISTENT_INTEGRAL;
        return true;
    };

    return modify(key, persist_if_not_expired);
}

auto DBShard::ttl(std::string_view key) -> std::optional<MilliSeconds>
{
    auto ttl_if_not_expired = [&] (DashTable<Entry>::Segment& segment, auto it) -> std::optional<MilliSeconds> 
    {
        if (it == segment.end())
            return {};

        if (TTLManager::is_expired(it->second.expire_at)) {
            segment.erase(it);
            return {};
        }

        if (TTLManager::is_persist(it->second.expire_at))
            return TTLManager::PERSISTENT_DURATION;

        return TTLManager::ttl(it->second.expire_at);
    };

    return modify(key, ttl_if_not_expired);
}

} // namespace spg::redis
