#include <sponge/random.h>

#include <random>

namespace spg {

auto random::engine() -> std::mt19937&
{
    thread_local std::mt19937 engine{ std::random_device{}() };
    return engine;
}

} // namespace spg
