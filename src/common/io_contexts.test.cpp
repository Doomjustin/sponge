#include <sponge/io_contexts.h>

#include <atomic>
#include <future>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace spg;
using namespace std::literals;

TEST_CASE("IOContexts 应报告配置的池大小", "[common][io_contexts]")
{
    IOContexts pool{ 3 };

    REQUIRE(pool.size() == 3);
}

TEST_CASE("IOContexts 应执行投递的任务", "[common][io_contexts]")
{
    IOContexts pool{ 2 };
    std::promise<void> done;
    auto done_future = done.get_future();
    std::atomic<bool> ran{ false };

    boost::asio::post(pool[0], [&ran, &done]() {
        ran.store(true, std::memory_order_relaxed);
        done.set_value();
    });

    auto status = done_future.wait_for(1s);

    REQUIRE(status == std::future_status::ready);
    REQUIRE(ran.load(std::memory_order_relaxed));
}

TEST_CASE("IOContexts 多个上下文均应能执行任务", "[common][io_contexts]")
{
    IOContexts pool{ 2 };
    std::promise<void> done0;
    std::promise<void> done1;
    auto fut0 = done0.get_future();
    auto fut1 = done1.get_future();
    std::atomic<bool> ran0{ false };
    std::atomic<bool> ran1{ false };

    boost::asio::post(pool[0], [&]() {
        ran0.store(true, std::memory_order_relaxed);
        done0.set_value();
    });

    boost::asio::post(pool[1], [&]() {
        ran1.store(true, std::memory_order_relaxed);
        done1.set_value();
    });

    REQUIRE(fut0.wait_for(1s) == std::future_status::ready);
    REQUIRE(fut1.wait_for(1s) == std::future_status::ready);
    REQUIRE(ran0.load(std::memory_order_relaxed));
    REQUIRE(ran1.load(std::memory_order_relaxed));
}

TEST_CASE("IOContexts 析构时应安全完成并返回", "[common][io_contexts]")
{
    auto start = std::chrono::steady_clock::now();
    {
        IOContexts pool{ 1 };
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    REQUIRE(elapsed < 1s);
}

