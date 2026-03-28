#include <sponge/redis/application_context.h>

namespace spg::redis {

ApplicationContext::ApplicationContext(Size count, std::string_view aof_filename)
    : io_context_pool_{ count },
      aof_{ aof_filename }
{
    pools_.reserve(count);
    resources_.reserve(count);
    shards_.reserve(count);

    for (Size i = 0; i < count; ++i) {
        pools_.emplace_back(std::make_unique<Pool>());
        resources_.emplace_back(std::make_unique<TrackingMemoryResource>(pools_.back().get()));
        shards_.emplace_back(std::make_unique<DBShard>(resources_.back().get()));
    }
}

constexpr auto ApplicationContext::data_size() const noexcept -> Size
{
    auto total_size = Size{ 0 };
    for (const auto& shard : shards_)
        total_size += shard->size();
    
    return total_size;
}

constexpr auto ApplicationContext::total_used_memory() const noexcept -> std::size_t
{
    Size total = 0;
    for (Size i = 0; i < size(); ++i)
        total += used_memory(i);

    return total;
}

} // namespace spg::redis
