#include <sponge/io_contexts.h>

#include <atomic>
#include <future>
#include <thread>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace spg;
using namespace std::literals;

TEST_CASE("IOContexts 报告配置的大小", "[io_contexts]")
{
    IOContexts pool{ 3 };

    REQUIRE(pool.size() == 3);
}

TEST_CASE("IOContexts 执行投递的任务", "[io_contexts]")
{
    IOContexts pool{ 2 };
    std::promise<void> done;
    auto done_future = done.get_future();
    std::atomic<bool> ran{ false };

    // run() 是阻塞的，必须放到后台线程
    std::thread runner([&pool] { pool.run(); });

    boost::asio::post(pool[0], [&ran, &done]() {
        ran.store(true, std::memory_order_relaxed);
        done.set_value();
    });

    auto status = done_future.wait_for(1s);

    // 无论断言结果如何都先停止，避免 join() 死锁
    pool.stop();
    runner.join();

    REQUIRE(status == std::future_status::ready);
    REQUIRE(ran.load(std::memory_order_relaxed));
}

TEST_CASE("IOContexts stop 重置 work guard 后 run 正常返回", "[io_contexts]")
{
    IOContexts pool{ 1 };
    std::atomic<bool> run_returned{ false };

    std::thread runner([&pool, &run_returned] {
        pool.run();
        run_returned.store(true, std::memory_order_relaxed);
    });

    pool.stop(); // 重置 work guard → io_context 空闲时 run() 返回
    runner.join();

    REQUIRE(run_returned.load(std::memory_order_relaxed));
}

TEST_CASE("IOContexts force_stop 后投递的新任务不执行", "[io_contexts]")
{
    IOContexts pool{ 1 };
    std::atomic<bool> executed{ false };

    std::thread runner([&pool] { pool.run(); });

    pool.force_stop();   // 立即停止 io_context，run() 尽快返回
    runner.join();       // 确认 run() 已返回，上下文已停止

    // 此时 io_context 已停止，新投递的任务不会被执行
    boost::asio::post(pool[0], [&executed]() {
        executed.store(true, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(50ms);
    REQUIRE_FALSE(executed.load(std::memory_order_relaxed));
}

