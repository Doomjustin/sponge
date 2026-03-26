#include "db_shard.h"

namespace spg::redis {

DBShard::DBShard(MemoryResource* resource)
  : resource_{ resource }, 
    tables_{ resource }
{
}

auto DBShard::get(std::string_view key, UseStringT t) -> std::optional<std::string_view>
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return {};
        }

        if (std::holds_alternative<String>(entry->value))
            return std::get<String>(entry->value);
    }

    return {};
}

auto DBShard::get(std::string_view key, UseIntegralT t) -> std::optional<Integral>
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return {};
        }

        if (std::holds_alternative<Integral>(entry->value))
            return std::get<Integral>(entry->value);
    }

    return {};
}

void DBShard::set(std::string_view key, std::string_view value)
{
    Entry entry{ .value = String{ value, resource_ },
                 .expire_at = TTLManager::PERSISTENT_INTEGRAL };
                 
    tables_.set(key, std::move(entry));
}

void DBShard::set(std::string_view key, Integral value)
{
    Entry entry{ .value = value, .expire_at = TTLManager::PERSISTENT_INTEGRAL };
    tables_.set(key, std::move(entry));
}

auto DBShard::erase(std::string_view key) -> bool
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return false;
        }
    }

    return tables_.erase(key);
}

auto DBShard::holds(std::string_view key, UseStringT t) -> bool
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return false;
        }

        return std::holds_alternative<String>(entry->value);
    }

    return false;
}

auto DBShard::holds(std::string_view key, UseIntegralT t) -> bool
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return false;
        }

        return std::holds_alternative<Integral>(entry->value);
    }

    return false;
}

auto DBShard::exists(std::string_view key) -> bool
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return false;
        }

        return true;
    }

    return false;
}

auto DBShard::type_of(std::string_view key) -> ValueType
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return ValueType::NotExist;
        }

        // 无论是string还是int都返回String类型，保持和redis一致
        if (std::holds_alternative<String>(entry->value))
            return ValueType::String;

        if (std::holds_alternative<Integral>(entry->value))
            return ValueType::String;
    }

    return ValueType::NotExist;
}

auto DBShard::expire(std::string_view key, MilliSeconds ttl) -> bool
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return false;
        }

        entry->expire_at = TTLManager::expire_at(ttl, return_integral);
        return true;
    }

    return false;
}

auto DBShard::persist(std::string_view key) -> bool
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return false;
        }

        entry->expire_at = TTLManager::PERSISTENT_INTEGRAL;
        return true;
    }

    return false;
}

auto DBShard::ttl(std::string_view key) -> std::optional<MilliSeconds>
{
    if (auto* entry = tables_.get(key)) {
        if (TTLManager::is_expired(entry->expire_at)) {
            tables_.erase(key);
            return {};
        }

        if (TTLManager::is_persist(entry->expire_at))
            return TTLManager::PERSISTENT_DURATION;

        return TTLManager::ttl(entry->expire_at);
    }

    return {};
}

} // namespace spg::redis
