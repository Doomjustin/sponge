#include <sponge/redis/application.h>

#include <memory_resource>

#include <sponge/redis/server.h>

namespace spg::redis {

Application::Application()
{
   std::pmr::set_default_resource(&resource_);
}

Application::~Application()
{
    std::pmr::set_default_resource(default_resource_);
}

void Application::execute()
{
    // TODO: 允许用户配置线程数和 AOF 文件路径
    Server server{ "0.0.0.0", "26379", 12 };
    server.run();
}

} // namespace spg::redis
