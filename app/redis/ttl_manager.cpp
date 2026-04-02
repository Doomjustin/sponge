#include "ttl_manager.h"

namespace spg::redis {

auto TTLManager::ttl(std::int64_t expire_at) -> std::optional<int64_t>
{
    if (expire_at == PERSISTENT)
        return PERSISTENT;

    auto current = now();

    if (current >= expire_at)
        return 0;

    return expire_at - current;
}

auto TTLManager::is_expired(std::int64_t expire_at) noexcept -> bool
{
    if (is_persist(expire_at))
        return false; // 负数表示持久化

    return now() >= expire_at;
}

auto TTLManager::expire_at(Milliseconds ttl) noexcept -> TimePoint
{
    if (ttl.count() < 0)
        return PERSISTENT_TIME_POINT;

    return Clock::now() + ttl;
}

auto TTLManager::expire_at(int64_t ttl_ms) noexcept -> std::int64_t
{
    if (ttl_ms < 0)
        return PERSISTENT; // 负数表示持久化

    auto expire_at = now() + ttl_ms;
    if (expire_at < 0) // 防止溢出
        return PERSISTENT;
    
    return expire_at;
}

} // namespace spg::redis
