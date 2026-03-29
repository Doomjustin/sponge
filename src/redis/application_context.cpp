#include <sponge/redis/application_context.h>

namespace spg::redis {

ApplicationContext::ApplicationContext(Size count, std::string_view aof_filename)
    : io_context_pool_{ count },
      aof_{ aof_filename }
{
    shards_.reserve(count);
    for (Size i = 0; i < count; ++i)
        shards_.emplace_back(std::make_unique<DBShard>());
}

constexpr auto ApplicationContext::data_size() const noexcept -> Size
{
    auto total_size = Size{ 0 };
    for (const auto& shard : shards_)
        total_size += shard->size();
    
    return total_size;
}

} // namespace spg::redis
