#ifndef SPONGE_REDIS_TTL_MANAGER_H
#define SPONGE_REDIS_TTL_MANAGER_H

#include <chrono>
#include <cstdint>

namespace spg::redis {

struct ReturnIntegralT {};

static constexpr ReturnIntegralT return_integral{};

class TTLManager {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    static constexpr TimePoint PERSISTENT_TIME_POINT = TimePoint::max();
    static constexpr std::int64_t PERSISTENT_INTEGRAL = -1;
    static constexpr Milliseconds PERSISTENT_DURATION = Milliseconds{ -1 };

    static auto ttl(std::int64_t expire_at) -> std::optional<Milliseconds>;

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

    static auto expire_at(Milliseconds ttl, ReturnIntegralT t) noexcept -> std::int64_t;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_TTL_MANAGER_H
