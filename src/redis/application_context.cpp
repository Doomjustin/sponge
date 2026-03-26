#include "application_context.h"

namespace spg::redis {

ApplicationContext::ApplicationContext(Size count)
    : io_context_pool(count)
{
    pools.reserve(count);
    resources.reserve(count);
    shards.reserve(count);

    for (Size i = 0; i < count; ++i) {
        pools.emplace_back(std::make_unique<Pool>());
        resources.emplace_back(std::make_unique<TrackingMemoryResource>(pools.back().get()));
        shards.emplace_back(resources.back().get());
    }
}

constexpr auto ApplicationContext::data_size() const noexcept -> Size
{
    auto total_size = Size{ 0 };
    for (const auto& shard : shards)
        total_size += shard.size();
    
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
