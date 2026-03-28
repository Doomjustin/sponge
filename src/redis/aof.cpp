#include <sponge/redis/aof.h>

namespace spg::redis {

AOF::AOF(std::string_view filename)
    : file_{ filename },
    io_context_{ 1 },
    work_guard_{ boost::asio::make_work_guard(io_context_) },
    thread_{ [this] { io_context_.run(); } }
{}

AOF::~AOF()
{
    // stopping_.store(true, std::memory_order_release);
    work_guard_.reset();
    io_context_.stop();

    if (thread_.joinable())
        thread_.join();

    if (file_.sync())
        SPDLOG_INFO("AOF file synced successfully on shutdown.");
    else
        SPDLOG_ERROR("Failed to sync AOF file on shutdown.");
}

void AOF::append(std::string command)
{
    // if (stopping_.load(std::memory_order_acquire))
    //     return;

    auto task = [this, command = std::move(command)]() 
    {
        // if (stopping_.load(std::memory_order_acquire))
        //     return;

        while (!file_.append(std::as_bytes(std::span{command}))) {
            // if (stopping_.load(std::memory_order_acquire))
            //     return;

            if (is_healthy()) {
                // 记录错误日志，标记文件状态为不健康
                SPDLOG_CRITICAL("AOF Write Error");
                healthy_.store(false, std::memory_order_release);
            }

            using namespace std::literals;
            std::this_thread::sleep_for(1s); // 写入失败时，等待一段时间后重试
        }

        file_.flush(); // 确保数据被写入磁盘

        if (!is_healthy()) {
            SPDLOG_INFO("AOF is recovered.");
            healthy_.store(true, std::memory_order_release);
        }
    };

    boost::asio::post(io_context_, task);
}

} // namespace spg::redis
