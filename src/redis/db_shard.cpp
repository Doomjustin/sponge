#include <sponge/redis/db_shard.h>

namespace spg::redis {

DBShard::DBShard(MemoryResource* resource)
  : resource_{ resource }, 
    tables_{ resource }
{}

} // namespace spg::redis
