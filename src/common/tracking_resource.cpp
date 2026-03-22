#include <sponge/tracking_resource.h>

#include <atomic>
#include <cstddef>
#include <memory_resource>

#include <gsl/gsl>

namespace spg {

TrackingMemoryResource::TrackingMemoryResource(gsl::not_null<std::pmr::memory_resource*> upstream)
    : upstream_{ upstream }
{
}

auto TrackingMemoryResource::do_allocate(std::size_t bytes, std::size_t alignment) -> void*
{
    void* ptr = upstream_->allocate(bytes, alignment);
    total_allocated_.fetch_add(bytes, std::memory_order_relaxed);
    return ptr;
}

void TrackingMemoryResource::do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment)
{
    upstream_->deallocate(ptr, bytes, alignment);
    total_allocated_.fetch_sub(bytes, std::memory_order_relaxed);
}

} // namespace spg
