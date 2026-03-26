#include "ttl_manager.h"

namespace spg::redis {

auto TTLManager::ttl(std::int64_t expire_at) -> std::optional<Milliseconds>
{
    if (expire_at == PERSISTENT_INTEGRAL)
        return PERSISTENT_DURATION;

    auto now =
        std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();

    if (now >= expire_at)
        return Milliseconds{ 0 };

    return Milliseconds{ expire_at - now };
}

auto TTLManager::is_expired(std::int64_t expire_at) noexcept -> bool
{
    if (is_persist(expire_at))
        return false; // 负数表示持久化

    auto now =
        std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
    return now >= expire_at;
}

auto TTLManager::expire_at(Milliseconds ttl) noexcept -> TimePoint
{
    if (ttl.count() < 0)
        return PERSISTENT_TIME_POINT;

    return Clock::now() + ttl;
}

auto TTLManager::expire_at(Milliseconds ttl, ReturnIntegralT t) noexcept -> std::int64_t
{
    if (ttl.count() < 0)
        return PERSISTENT_INTEGRAL; // 负数表示持久化

    auto expire_at = Clock::now() + ttl;
    return std::chrono::duration_cast<Milliseconds>(expire_at.time_since_epoch()).count();
}

} // namespace spg::redis
