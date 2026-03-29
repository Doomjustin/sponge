#ifndef SPONGE_REDIS_AOF_H
#define SPONGE_REDIS_AOF_H

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include <sponge/writable_file.h>

#include "db_shard.h"

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
        return healthy_.load(std::memory_order_relaxed);
    }

    auto is_rewriting() const -> bool
    {
        return is_rewriting_.load(std::memory_order_relaxed);
    }

    void background_rewrite(std::span<const std::unique_ptr<DBShard>> shards);

    void reset();

    auto size() const noexcept -> size_t
    {
        return std::filesystem::file_size(file_.path());
    }

    auto last_rewrite_size() const noexcept -> size_t
    {
        return last_rewrite_size_.load(std::memory_order_relaxed);
    }

private:
    StdWritableFile file_;
    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::thread thread_;
    std::atomic<bool> healthy_{ true };
    std::atomic<bool> stopping_{ false };
    std::atomic<bool> is_rewriting_{ false };
    std::pmr::string rewrite_buffer_;
    std::atomic<size_t> last_rewrite_size_{ 0 };

    void swap_rewrite_file(const std::filesystem::path& tmp_file);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_AOF_H
