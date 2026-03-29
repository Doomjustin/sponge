#ifndef SPONGE_REDIS_AOF_H
#define SPONGE_REDIS_AOF_H

#include <atomic>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include <sponge/writable_file.h>

namespace spg::redis {

class AOF {
public:
    explicit AOF(std::string_view filename);

    AOF(const AOF&) = delete;
    auto operator=(const AOF&) -> AOF& = delete;

    AOF(AOF&&) = delete;
    auto operator=(AOF&&) -> AOF& = delete;

    ~AOF();

    void append(std::pmr::string command);

    auto is_healthy() const -> bool
    {
        return healthy_.load(std::memory_order_acquire);
    }

private:
    StdWritableFile file_;
    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::thread thread_;
    std::atomic<bool> healthy_{ true };
    std::atomic<bool> stopping_{ false };
};

} // namespace spg::redis

#endif // SPONGE_REDIS_AOF_H
