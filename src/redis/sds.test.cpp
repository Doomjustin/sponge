#include "sds.h"

#include <cstddef>
#include <memory_resource>
#include <string_view>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

using namespace spg::redis;

namespace {

class CountingResource : public std::pmr::memory_resource {
public:
    [[nodiscard]]
    auto allocations() const noexcept -> std::size_t { return alloc_count_; }

    [[nodiscard]]
    auto deallocations() const noexcept -> std::size_t { return dealloc_count_; }

    [[nodiscard]]
    auto has_size_mismatch() const noexcept -> bool { return size_mismatch_; }

    [[nodiscard]]
    auto last_allocation_size() const noexcept -> std::size_t { return last_allocation_size_; }

private:
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override
    {
        auto* ptr = upstream_.allocate(bytes, alignment);
        allocated_[ptr] = bytes;
        last_allocation_size_ = bytes;
        ++alloc_count_;
        return ptr;
    }

    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override
    {
        auto it = allocated_.find(ptr);
        if (it == allocated_.end() || it->second != bytes)
            size_mismatch_ = true;
        else
            allocated_.erase(it);

        ++dealloc_count_;
        upstream_.deallocate(ptr, bytes, alignment);
    }

    [[nodiscard]]
    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override
    {
        return this == &other;
    }

    std::pmr::memory_resource& upstream_{ *std::pmr::new_delete_resource() };
    std::unordered_map<void*, std::size_t> allocated_{};
    std::size_t alloc_count_{ 0 };
    std::size_t dealloc_count_{ 0 };
    std::size_t last_allocation_size_{ 0 };
    bool size_mismatch_{ false };
};

// RAII helper：将 CountingResource 设为默认 PMR 资源，析构时还原。
struct ResourceGuard {
    explicit ResourceGuard(CountingResource& res)
      : prev_{ std::pmr::set_default_resource(&res) }
    {}

    ~ResourceGuard()
    {
        std::pmr::set_default_resource(prev_);
    }

    ResourceGuard(const ResourceGuard&) = delete;
    auto operator=(const ResourceGuard&) -> ResourceGuard& = delete;

    std::pmr::memory_resource* prev_;
};

} // namespace

TEST_CASE("SDS create stores string and null terminator", "[sds][manager]")
{
    CountingResource resource{};
    ResourceGuard guard{ resource };
    SDS manager{};

    constexpr std::string_view text{ "hello" };
    auto* sds = manager.create(text);

    REQUIRE(sds != nullptr);
    REQUIRE(std::string_view{ sds } == text);
    REQUIRE(sds[text.size()] == '\0');

    manager.destroy(sds);
    REQUIRE(resource.allocations() == 1);
    REQUIRE(resource.deallocations() == 1);
    REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("SDS create with explicit capacity keeps content", "[sds][manager]")
{
    CountingResource resource{};
    ResourceGuard guard{ resource };
    SDS manager{};

    constexpr std::string_view text{ "abc" };
    auto* sds = manager.create(text, 32);

    REQUIRE(std::string_view{ sds } == text);
    REQUIRE(sds[text.size()] == '\0');
    REQUIRE(resource.last_allocation_size() == 3 + 32 + 1);

    manager.destroy(sds);
    REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("SDS chooses Type8 below 256 capacity", "[sds][manager]")
{
    CountingResource resource{};
    ResourceGuard guard{ resource };
    SDS manager{};

    auto* sds = manager.create("x", 255);

    REQUIRE(resource.last_allocation_size() == 3 + 255 + 1);

    manager.destroy(sds);
    REQUIRE_FALSE(resource.has_size_mismatch());
}

TEST_CASE("SDS chooses Type16 at 256 capacity", "[sds][manager]")
{
    CountingResource resource{};
    ResourceGuard guard{ resource };
    SDS manager{};

    auto* sds = manager.create("x", 256);

    REQUIRE(resource.last_allocation_size() == 5 + 256 + 1);

    manager.destroy(sds);
    REQUIRE_FALSE(resource.has_size_mismatch());
}
