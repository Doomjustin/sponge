#include <sponge/tracking_resource.h>

#include <cstddef>
#include <memory_resource>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("TrackingMemoryResource tracks allocated memory",
          "[spg_base_tracking_resource][used_memory]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    constexpr std::size_t first_bytes{ 64 };
    constexpr std::size_t second_bytes{ 16 };
    auto* first{ resource.allocate(first_bytes, alignof(std::max_align_t)) };
    auto* second{ resource.allocate(second_bytes, alignof(std::max_align_t)) };

    REQUIRE(resource.used_memory() == first_bytes + second_bytes);

    resource.deallocate(second, second_bytes, alignof(std::max_align_t));
    REQUIRE(resource.used_memory() == first_bytes);

    resource.deallocate(first, first_bytes, alignof(std::max_align_t));
    REQUIRE(resource.used_memory() == 0);
}
