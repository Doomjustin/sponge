#ifndef SPONGE_REDIS_APPLICATION_CONTEXT_H
#define SPONGE_REDIS_APPLICATION_CONTEXT_H

#include <cstddef>
#include <memory>
#include <vector>

#include <boost/asio/io_context.hpp>

#include <sponge/io_contexts.h>
#include <sponge/tag.h>
#include <sponge/tracking_resource.h>

#include "aof.h"
#include "db_shard.h"

namespace spg::redis {

struct HashedKey {
    size_t value;
};

constexpr auto hashed_key(size_t value) -> HashedKey
{
    return { value };
}

class ApplicationContext {
public:
    using Size = size_t;

    ApplicationContext(Size count, std::string_view aof_filename);

    ApplicationContext(const ApplicationContext&) = delete;
    auto operator=(const ApplicationContext&) -> ApplicationContext& = delete;

    ApplicationContext(ApplicationContext&&) = delete;
    auto operator=(ApplicationContext&&) -> ApplicationContext& = delete;

    ~ApplicationContext() = default;

    [[nodiscard]]
    constexpr auto data_size() const noexcept -> Size;

    auto shard(HashedKey key) noexcept -> DBShard&
    {
        return *shards_[key.value % shards_.size()];
    }

    auto shard(Size index) noexcept -> DBShard& { return *shards_[index]; }

    auto io_context(HashedKey key) noexcept -> boost::asio::io_context&
    {
        return io_context_pool_[key.value % io_context_pool_.size()];
    }

    auto io_context(Size id) noexcept -> boost::asio::io_context&
    {
        return io_context_pool_[id];
    }

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return shards_.size();
    }

    auto aof() noexcept -> AOF&
    {
        return aof_;
    }

    auto shards() const noexcept -> std::span<const std::unique_ptr<DBShard>>
    {
        return shards_;
    }

private:
    IOContexts io_context_pool_;
    std::vector<std::unique_ptr<DBShard>> shards_;
    AOF aof_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_APPLICATION_CONTEXT_H
