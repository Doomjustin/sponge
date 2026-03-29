#ifndef SPONGE_REDIS_APPLICATION_H
#define SPONGE_REDIS_APPLICATION_H

#include <memory_resource>

#include <sponge/tracking_resource.h>

namespace spg::redis {

// 为了确保track_resource在所有组件之前构造，需要在server上加一层壳
class Application {
public:
    Application();

    ~Application();

    void execute();

    auto used_memory() const noexcept -> std::size_t 
    {
        return resource_.used_memory(); 
    }

private:
    std::pmr::memory_resource* default_resource_ = std::pmr::get_default_resource();
    TrackingMemoryResource resource_{ std::pmr::new_delete_resource() };
};

} // namespace spg::redis

#endif // SPONGE_REDIS_APPLICATION_H
