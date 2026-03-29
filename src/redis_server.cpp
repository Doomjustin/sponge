#include <cstdlib>

#include <sponge/redis/server.h>
#include <sponge/tracking_resource.h>

int main(int argc, char* argv[])
{
    std::pmr::synchronized_pool_resource pool_;
    spg::TrackingMemoryResource resource_{ &pool_ };
    std::pmr::set_default_resource(&resource_);

    using namespace spg::redis;

    Server server{ "0.0.0.0", "26379", 12 };
    server.run();

    return EXIT_SUCCESS;
}