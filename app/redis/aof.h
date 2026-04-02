#ifndef SPONGE_REDIS_AOF_H
#define SPONGE_REDIS_AOF_H

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <span>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

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
        return writer_.is_healthy();
    }

    auto is_rewriting() const -> bool
    {
        return rewriter_.is_rewriting();
    }

    void background_rewrite(std::span<const std::unique_ptr<DBShard>> shards);

    void reset();

    auto size() const noexcept -> size_t
    {
        return std::filesystem::file_size(writer_.path());
    }

    auto last_rewrite_size() const noexcept -> size_t
    {
        return last_rewrite_size_.load(std::memory_order_relaxed);
    }

private:
    class AofCommandEncoder {
    public:
        static void dump_shards_to_file(std::span<const std::unique_ptr<DBShard>> shards,
                                        const std::filesystem::path& path);

    private:
        static void encode_request(std::pmr::string& buffer,
                                   std::initializer_list<std::string_view> args);
        static void handle_chunk(const DBShard::LockedReadView& chunk,
                                 std::pmr::string& buffer);
    };

    class AofWriter {
    public:
        explicit AofWriter(std::filesystem::path path);

        ~AofWriter();

        auto append(std::string_view message) -> bool;

        auto sync() -> bool;

        void close();

        void reopen(std::filesystem::path path);

        auto reset_file(std::string_view backup_suffix) -> bool;

        auto replace_from(const std::filesystem::path& tmp_path) -> bool;

        void stop();

        [[nodiscard]]
        auto is_healthy() const -> bool
        {
            return healthy_.load(std::memory_order_relaxed);
        }

        [[nodiscard]]
        auto path() const -> std::filesystem::path
        {
            std::lock_guard<std::mutex> lock{ mutex_ };
            return path_;
        }

    private:
        mutable std::mutex mutex_;
        std::filesystem::path path_;
        int fd_{ -1 };
        std::atomic<bool> healthy_{ true };
        std::atomic<bool> stopping_{ false };
    };

    class AofRewriter {
    public:
        using OnComplete = std::function<void(const std::filesystem::path&)>;

        AofRewriter() = default;

        AofRewriter(const AofRewriter&) = delete;
        auto operator=(const AofRewriter&) -> AofRewriter& = delete;

        AofRewriter(AofRewriter&&) = delete;
        auto operator=(AofRewriter&&) -> AofRewriter& = delete;

        ~AofRewriter();

        auto is_rewriting() const -> bool
        {
            return is_rewriting_.load(std::memory_order_relaxed);
        }

        auto try_start(std::span<const std::unique_ptr<DBShard>> shards,
                       std::filesystem::path tmp_path,
                       OnComplete on_complete) -> bool;

        void stop();

    private:
        std::jthread worker_;
        std::atomic<bool> is_rewriting_{ false };
    };

    AofWriter writer_;
    boost::asio::io_context io_context_;
    boost::asio::steady_timer sync_timer_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::thread thread_;
    std::atomic<bool> stopping_{ false };
    AofRewriter rewriter_;
    std::pmr::string rewrite_buffer_;
    std::atomic<size_t> last_rewrite_size_{ 0 };

    void schedule_periodic_sync();
    void capture_rewrite_delta(std::pmr::string command);
    void swap_rewrite_file(const std::filesystem::path& tmp_file);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_AOF_H
