#include <sponge/io_contexts.h>

#include <atomic>
#include <future>
#include <thread>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace spg;
using namespace std::literals;

TEST_CASE("IOContextPool reports configured size", "[spg_base_io_contexts][size]")
{
    IOContexts pool{ 3 };

    REQUIRE(pool.size() == 3);
}

TEST_CASE("IOContextPool executes posted work", "[spg_base_io_contexts][post]")
{
    IOContexts pool{ 2 };
    std::promise<void> done;
    auto done_future{ done.get_future() };
    std::atomic<bool> ran{ false };

    boost::asio::post(pool[0], [&ran, &done]() {
        ran.store(true, std::memory_order_relaxed);
        done.set_value();
    });

    REQUIRE(done_future.wait_for(1s) == std::future_status::ready);
    REQUIRE(ran.load(std::memory_order_relaxed));
}

TEST_CASE("IOContextPool stop prevents newly posted work from running",
          "[spg_base_io_contexts][stop]")
{
    IOContexts pool{ 1 };
    std::atomic<bool> executed{ false };

    pool.stop();
    boost::asio::post(pool[0], [&executed]() { executed.store(true, std::memory_order_relaxed); });

    std::this_thread::sleep_for(100ms);
    REQUIRE_FALSE(executed.load(std::memory_order_relaxed));
}
