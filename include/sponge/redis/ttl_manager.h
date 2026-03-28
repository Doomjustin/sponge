#ifndef SPONGE_REDIS_TTL_MANAGER_H
#define SPONGE_REDIS_TTL_MANAGER_H

#include <chrono>
#include <cstdint>

namespace spg::redis {

class TTLManager {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    static constexpr TimePoint PERSISTENT_TIME_POINT = TimePoint::max();
    static constexpr std::int64_t PERSISTENT_INTEGRAL = -1;
    static constexpr Milliseconds PERSISTENT_DURATION = Milliseconds{ -1 };

    static auto now() -> std::int64_t
    {
        return std::chrono::duration_cast<Milliseconds>(Clock::now().time_since_epoch()).count();
    }

    static auto ttl(std::int64_t expire_at) -> std::optional<int64_t>;

    static constexpr auto is_persist(TimePoint tp) noexcept -> bool
    {
        return tp == PERSISTENT_TIME_POINT;
    }

    static constexpr auto is_persist(std::int64_t expire_at) noexcept -> bool
    {
        return expire_at == PERSISTENT_INTEGRAL;
    }

    static auto is_expired(TimePoint expire_at) noexcept -> bool
    {
        return !is_persist(expire_at) && Clock::now() >= expire_at;
    }

    static auto is_expired(std::int64_t expire_at) noexcept -> bool;

    static auto expire_at(Milliseconds ttl) noexcept -> TimePoint;

    static auto expire_at(int64_t ttl_ms) noexcept -> std::int64_t;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_TTL_MANAGER_H
