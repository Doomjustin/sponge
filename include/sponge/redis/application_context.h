#ifndef SPONGE_REDIS_APPLICATION_CONTEXT_H
#define SPONGE_REDIS_APPLICATION_CONTEXT_H

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <vector>

#include <boost/asio/io_context.hpp>

#include <sponge/io_contexts.h>
#include <sponge/tracking_resource.h>

#include "db_shard.h"

namespace spg::redis {

class ApplicationContext {
public:
    using Size = std::size_t;

    explicit ApplicationContext(Size count);

    ApplicationContext(const ApplicationContext&) = delete;
    auto operator=(const ApplicationContext&) -> ApplicationContext& = delete;

    ApplicationContext(ApplicationContext&&) = delete;
    auto operator=(ApplicationContext&&) -> ApplicationContext& = delete;

    ~ApplicationContext() = default;

    void run()
    {
        io_context_pool_.run();
    }

    void stop()
    {
        io_context_pool_.stop();
    }

    [[nodiscard]]
    constexpr auto data_size() const noexcept -> Size;

    auto shard(Size key_hash, ByHashT by_hash) noexcept -> DBShard&
    {
        return *shards_[key_hash % shards_.size()];
    }

    auto shard(Size index) noexcept -> DBShard& { return *shards_[index]; }

    auto io_context(Size key_hash, ByHashT by_hash) noexcept -> boost::asio::io_context&
    {
        return io_context_pool_[key_hash % io_context_pool_.size()];
    }

    auto io_context(std::size_t id) noexcept -> boost::asio::io_context&
    {
        return io_context_pool_[id];
    }

    auto resource(Size index) noexcept -> TrackingMemoryResource*
    {
        return resources_[index].get();
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return shards_.size();
    }

    // 非线程安全，调用者必须保证线程安全
    [[nodiscard]]
    auto used_memory(Size index) const noexcept -> std::size_t
    {
        return resources_[index]->used_memory();
    }

    // 非线程安全，调用者必须保证线程安全
    [[nodiscard]]
    constexpr auto total_used_memory() const noexcept -> std::size_t;

private:
    using Pool = std::pmr::synchronized_pool_resource;

    IOContexts io_context_pool_;
    std::vector<std::unique_ptr<Pool>> pools_;
    std::vector<std::unique_ptr<TrackingMemoryResource>> resources_;
    std::vector<std::unique_ptr<DBShard>> shards_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_APPLICATION_CONTEXT_H
