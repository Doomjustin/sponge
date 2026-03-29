#include <sponge/redis/aof.h>

#include <atomic>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <sponge/redis/db_shard.h>
#include <sponge/redis/ttl_manager.h>
#include <sponge/tag.h>
#include <sponge/writable_file.h>

namespace fs = std::filesystem;

namespace spg::redis {
namespace {

void encode_request(std::pmr::string& buffer, std::initializer_list<std::string_view> args)
{
    fmt::format_to(std::back_inserter(buffer), "*{}\r\n", args.size());
    for (const auto& arg : args) 
        fmt::format_to(std::back_inserter(buffer), "${}\r\n{}\r\n", arg.size(), arg);
}

void dump_shards_to_file(std::span<const std::unique_ptr<DBShard>> shards, const std::filesystem::path& path) 
{
    StdWritableFile file{ path };
    std::pmr::string buffer;
    buffer.reserve(1024 * 1024 * 8);

    for (const auto& shard : shards) {
        for (auto chunk : shard->range(read_only)) {
            for (const auto& [key, entry] : chunk) {
                if (TTLManager::is_expired(entry.expire_at))
                    continue;

                if (std::holds_alternative<DBShard::String>(entry.value)) {
                    auto ttl = TTLManager::ttl(entry.expire_at);
                    if (ttl) {
                        auto value = std::get<DBShard::String>(entry.value);
                        encode_request(buffer, { "SET", key, value });
                    }
                }
                else if (std::holds_alternative<DBShard::Integral>(entry.value)) {
                    auto ttl = TTLManager::ttl(entry.expire_at);
                    if (ttl) {
                        auto value = std::get<DBShard::Integral>(entry.value);
                        encode_request(buffer, { "SET", key, std::to_string(value) });
                    }
                }
                else if (std::holds_alternative<DBShard::ZSet>(entry.value)) {
                    auto ttl = TTLManager::ttl(entry.expire_at);
                    if (ttl) {
                        auto& zset = std::get<DBShard::ZSet>(entry.value);
                        // TODO: 暂时不支持遍历
                        // for (const auto& [member, score] : zset) {
                        //     encode_request(buffer, { "ZADD", key, std::to_string(score), member });
                        // }
                    }
                }
            }

            if (buffer.size() > 4 * 1024 * 1024) {
                file.append(std::as_bytes(std::span{buffer}));
                buffer.clear();
            }
        }
    }

    if (!buffer.empty()) 
        file.append(std::as_bytes(std::span{buffer}));

    file.sync();
    file.close();
}

} // namespace


AOF::AOF(std::string_view filename)
  : file_{ filename },
    io_context_{ 1 },
    work_guard_{ boost::asio::make_work_guard(io_context_) },
    thread_{ [this] { io_context_.run(); } }
{}

AOF::~AOF()
{
    stopping_.store(true, std::memory_order_relaxed);
    work_guard_.reset();
    io_context_.stop();

    if (thread_.joinable())
        thread_.join();

    if (file_.sync())
        SPDLOG_INFO("AOF file synced successfully on shutdown.");
    else
        SPDLOG_ERROR("Failed to sync AOF file on shutdown.");
}

void AOF::append(std::pmr::string command)
{
    if (stopping_.load(std::memory_order_relaxed))
        return;

    auto task = [this, command = std::move(command)]() 
    {
        if (stopping_.load(std::memory_order_relaxed))
            return;

        while (!file_.append(std::as_bytes(std::span{command}))) {
            if (stopping_.load(std::memory_order_relaxed))
                return;

            if (is_healthy()) {
                // 记录错误日志，标记文件状态为不健康
                SPDLOG_CRITICAL("AOF Write Error");
                healthy_.store(false, std::memory_order_relaxed);
            }

            using namespace std::literals;
            std::this_thread::sleep_for(1s); // 写入失败时，等待一段时间后重试
        }

        file_.flush(); // 确保数据被写入磁盘

        if (!is_healthy()) {
            SPDLOG_INFO("AOF is recovered.");
            healthy_.store(true, std::memory_order_relaxed);
        }
    };

    boost::asio::post(io_context_, task);
}

void AOF::background_rewrite(std::span<const std::unique_ptr<DBShard>> shards)
{
    auto task = [this, shards] -> void 
    {
        SPDLOG_INFO("AOF: Starting background rewrite...");
        is_rewriting_.store(true, std::memory_order_relaxed);

        auto tmp_path = file_.path().concat(".tmp");
        dump_shards_to_file(shards, tmp_path);
        swap_rewrite_file(tmp_path);

        SPDLOG_INFO("AOF: rewrite completed.");
    };

    std::thread thread{ task };
    thread.detach();
}

void AOF::reset()
{
    auto task = [this] 
    {
        healthy_.store(false, std::memory_order_relaxed);

        file_.sync();
        file_.close();
        
        auto flushed = file_.path().concat(".flushed");
        fs::rename(file_.path(), flushed);
        
        file_.reopen(file_.path());
        fs::remove(flushed);
        last_rewrite_size_.store(0, std::memory_order_relaxed);

        healthy_.store(true, std::memory_order_relaxed);
    };

    boost::asio::post(io_context_, task);
}

void AOF::swap_rewrite_file(const std::filesystem::path& tmp)
{
    auto task = [this, tmp]() 
    {
        file_.sync(); // 确保当前 AOF 文件的内容被写入磁盘
        file_.close();  
        fs::rename(tmp, file_.path());
        file_.reopen(file_.path());
        is_rewriting_.store(false, std::memory_order_relaxed);
    };

    boost::asio::post(io_context_, task);
}

} // namespace spg::redis
