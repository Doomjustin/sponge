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

TEST_CASE("TrackingMemoryResource tracks multiple allocations correctly",
          "[spg_base_tracking_resource][used_memory]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    constexpr std::size_t size1{ 256 };
    constexpr std::size_t size2{ 512 };

    auto* p1{ resource.allocate(size1, alignof(std::max_align_t)) };
    REQUIRE(resource.used_memory() == size1);

    auto* p2{ resource.allocate(size2, alignof(std::max_align_t)) };
    REQUIRE(resource.used_memory() == size1 + size2);

    resource.deallocate(p2, size2, alignof(std::max_align_t));
    REQUIRE(resource.used_memory() == size1);

    resource.deallocate(p1, size1, alignof(std::max_align_t));
    REQUIRE(resource.used_memory() == 0);
}

TEST_CASE("TrackingMemoryResource handles sequential allocation and deallocation",
          "[spg_base_tracking_resource]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    std::vector<void*> ptrs;
    constexpr std::size_t alloc_size{ 128 };
    constexpr int num_allocs{ 5 };

    // Allocate multiple blocks
    for (int i = 0; i < num_allocs; ++i) {
        ptrs.push_back(resource.allocate(alloc_size, alignof(std::max_align_t)));
    }

    REQUIRE(resource.used_memory() == alloc_size * num_allocs);

    // Deallocate in reverse order
    for (int i = num_allocs - 1; i >= 0; --i) {
        resource.deallocate(ptrs[i], alloc_size, alignof(std::max_align_t));
    }

    REQUIRE(resource.used_memory() == 0);
}

TEST_CASE("TrackingMemoryResource handles different allocation sizes",
          "[spg_base_tracking_resource]")
{
    TrackingMemoryResource resource{ std::pmr::new_delete_resource() };

    const std::size_t sizes[] = { 1, 10, 100, 1000, 256, 512 };
    std::vector<std::pair<void*, std::size_t>> allocations;

    std::size_t total{ 0 };
    for (auto size : sizes) {
        auto* ptr = resource.allocate(size, alignof(std::max_align_t));
        allocations.emplace_back(ptr, size);
        total += size;
    }

    REQUIRE(resource.used_memory() == total);

    // Deallocate all
    for (const auto& [ptr, size] : allocations) {
        resource.deallocate(ptr, size, alignof(std::max_align_t));
    }

    REQUIRE(resource.used_memory() == 0);
}
