#include <cstdlib>

#include <sponge/redis/server.h>

int main(int argc, char* argv[])
{
    using namespace spg::redis;
    Server server{ "0.0.0.0", "26379", 12 };
    server.run();

    return EXIT_SUCCESS;
}