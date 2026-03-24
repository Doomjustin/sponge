#ifndef SPONGE_REDIS_APPLICATION_CONTEXT_H
#define SPONGE_REDIS_APPLICATION_CONTEXT_H

#include <cstddef>
#include <memory_resource>
#include <vector>

#include <sponge/io_contexts.h>
#include <sponge/tracking_resource.h>

#include "db_shard.h"

namespace spg::redis {

struct ByHashT {};

static constexpr ByHashT by_hash{};

class ApplicationContext {
public:
    using Size = std::size_t;

    explicit ApplicationContext(Size count);

    ApplicationContext(const ApplicationContext&) = delete;
    auto operator=(const ApplicationContext&) -> ApplicationContext& = delete;

    ApplicationContext(ApplicationContext&&) = delete;
    auto operator=(ApplicationContext&&) -> ApplicationContext& = delete;

    ~ApplicationContext() = default;

    [[nodiscard]]
    constexpr auto data_size() const noexcept -> Size;

    auto db(Size key_hash, ByHashT by_hash) noexcept -> DBShard&
    {
        return shards[key_hash % shards.size()];
    }

    auto db(Size index) noexcept -> DBShard& { return shards[index]; }

    auto io_context(Size key_hash, ByHashT by_hash) noexcept -> boost::asio::io_context&
    {
        return io_context_pool[key_hash % io_context_pool.size()];
    }

    auto io_context(std::size_t id) noexcept -> boost::asio::io_context&
    {
        return io_context_pool[id];
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return shards.size();
    }

    [[nodiscard]]
    auto used_memory(Size index) const noexcept -> std::size_t
    {
        return resources[index]->used_memory();
    }

    [[nodiscard]]
    constexpr auto total_used_memory() const noexcept -> std::size_t;

private:
    using Pool = std::pmr::unsynchronized_pool_resource;

    IOContextPool io_context_pool;
    std::vector<std::unique_ptr<Pool>> pools;
    std::vector<std::unique_ptr<TrackingMemoryResource>> resources;
    std::pmr::vector<DBShard> shards;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_APPLICATION_CONTEXT_H
