#include <sponge/redis/aof.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

#include <fcntl.h>
#include <unistd.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <sponge/redis/db_shard.h>
#include <sponge/redis/ttl_manager.h>
#include <sponge/tag.h>
#include <sponge/writable_file.h>

namespace fs = std::filesystem;

namespace spg::redis {
void AOF::AofCommandEncoder::encode_request(std::pmr::string& buffer,
                                            std::initializer_list<std::string_view> args)
{
    fmt::format_to(std::back_inserter(buffer), "*{}\r\n", args.size());
    for (const auto& arg : args) 
        fmt::format_to(std::back_inserter(buffer), "${}\r\n{}\r\n", arg.size(), arg);
}

void AOF::AofCommandEncoder::handle_chunk(const DBShard::LockedReadView& chunk,
                                          std::pmr::string& buffer)
{
    for (const auto& [key, entry] : chunk) {
        if (TTLManager::is_expired(entry.expire_at))
            continue;

        if (std::holds_alternative<DBShard::String>(entry.value)) {
            auto ttl_opt = TTLManager::ttl(entry.expire_at);
            if (ttl_opt) {
                auto ttl = *ttl_opt;
                auto value = std::get<DBShard::String>(entry.value);

                if (ttl != TTLManager::PERSISTENT)
                    encode_request(buffer, { "SET", key, value, "EX", fmt::format("{}", ttl) });
                else
                    encode_request(buffer, { "SET", key, value });
            }
        }
        else if (std::holds_alternative<DBShard::Integral>(entry.value)) {
            auto ttl_opt = TTLManager::ttl(entry.expire_at);
            if (ttl_opt) {
                auto ttl = *ttl_opt;
                auto value = std::get<DBShard::Integral>(entry.value);
                if (ttl != TTLManager::PERSISTENT)
                    encode_request(buffer, { "SET", key, fmt::format("{}", value), "EX", fmt::format("{}", ttl) });
                else
                    encode_request(buffer, { "SET", key, fmt::format("{}", value) });
            }
        }
        else if (std::holds_alternative<DBShard::ZSet>(entry.value)) {
            auto ttl = TTLManager::ttl(entry.expire_at);
            if (ttl) {
                auto& zset = std::get<DBShard::ZSet>(entry.value);
                for (auto it = zset.begin(); it != zset.end(); ++it) {
                    auto [member, score] = *it;
                    encode_request(buffer, { "ZADD", key, fmt::format("{}", score), member });
                }
            }
        }
        else if (std::holds_alternative<DBShard::HashTable>(entry.value)) {
            auto ttl = TTLManager::ttl(entry.expire_at);
            if (ttl) {
                auto& hash_table = std::get<DBShard::HashTable>(entry.value);
                for (const auto& [field, value] : hash_table) {
                    if (ttl != TTLManager::PERSISTENT)
                        encode_request(buffer, { "HSET", key, field, value, "EX", fmt::format("{}", *ttl) });
                    else
                        encode_request(buffer, { "HSET", key, field, value });
                }
            }
        }
        else if (std::holds_alternative<DBShard::List>(entry.value)) {
            auto ttl = TTLManager::ttl(entry.expire_at);
            if (ttl) {
                auto& list = std::get<DBShard::List>(entry.value);
                for (const auto& element : list) {
                    if (ttl != TTLManager::PERSISTENT)
                        encode_request(buffer, { "RPUSH", key, element, "EX", fmt::format("{}", *ttl) });
                    else
                        encode_request(buffer, { "RPUSH", key, element });
                }
            }
        }
    }
}

void AOF::AofCommandEncoder::dump_shards_to_file(std::span<const std::unique_ptr<DBShard>> shards,
                                                 const std::filesystem::path& path)
{
    StdWritableFile file{ path };
    std::pmr::string buffer;
    buffer.reserve(1024 * 1024 * 8);

    for (const auto& shard : shards) {
        for (auto chunk : shard->range(read_only))
            handle_chunk(chunk, buffer);
    
        if (buffer.size() > 4 * 1024 * 1024) {
            file.append(std::as_bytes(std::span{buffer}));
            buffer.clear();
        }
    }

    if (!buffer.empty()) 
        file.append(std::as_bytes(std::span{buffer}));

    file.sync();
    file.close();
}


AOF::AOF(std::string_view filename)
  : writer_{ std::filesystem::path{ filename } },
    io_context_{ 1 },
    sync_timer_{ io_context_ },
    work_guard_{ boost::asio::make_work_guard(io_context_) },
    thread_{ [this] { io_context_.run(); } }
{
    schedule_periodic_sync();
}

void AOF::schedule_periodic_sync()
{
    sync_timer_.expires_after(std::chrono::seconds{ 1 });
    sync_timer_.async_wait([this](const boost::system::error_code& ec)
    {
        if (ec || stopping_.load(std::memory_order_relaxed))
            return;

        if (!writer_.sync() && writer_.is_healthy())
            SPDLOG_ERROR("Periodic AOF sync failed.");

        if (!stopping_.load(std::memory_order_relaxed))
            schedule_periodic_sync();
    });
}

AOF::~AOF()
{
    stopping_.store(true, std::memory_order_relaxed);
    rewriter_.stop();
    work_guard_.reset();
    io_context_.stop();

    if (thread_.joinable())
        thread_.join();

    if (writer_.sync())
        SPDLOG_INFO("AOF file synced successfully on shutdown.");
    else
        SPDLOG_ERROR("Failed to sync AOF file on shutdown.");

    writer_.stop();
}

void AOF::append(std::pmr::string command)
{
    if (stopping_.load(std::memory_order_relaxed))
        return;

    if (!writer_.append(command))
        return;

    capture_rewrite_delta(std::move(command));
}

void AOF::capture_rewrite_delta(std::pmr::string command)
{
    if (!rewriter_.is_rewriting())
        return;

    auto task = [this, command = std::move(command)]() mutable
    {
        if (rewriter_.is_rewriting())
            rewrite_buffer_.append(command);
    };

    boost::asio::post(io_context_, std::move(task));
}

void AOF::background_rewrite(std::span<const std::unique_ptr<DBShard>> shards)
{
    auto tmp_path = writer_.path();
    tmp_path += ".tmp";
    auto on_complete = [this](const std::filesystem::path& tmp)
    {
        swap_rewrite_file(tmp);
    };

    if (!rewriter_.try_start(shards, tmp_path, std::move(on_complete))) {
        SPDLOG_WARN("AOF rewrite already in progress, skipping new rewrite request.");
        return;
    }
}

void AOF::reset()
{
    auto task = [this] 
    {
        if (!writer_.reset_file(".flushed")) {
            SPDLOG_ERROR("AOF reset failed");
            return;
        }

        last_rewrite_size_.store(0, std::memory_order_relaxed);
    };

    boost::asio::post(io_context_, task);
}

void AOF::swap_rewrite_file(const std::filesystem::path& tmp)
{
    auto task = [this, tmp]() 
    {
        if (!rewrite_buffer_.empty()) {
            // 在重写过程中有新的命令写入，追加到重写文件末尾，确保不丢失数据
            StdWritableFile rewrite_file{ tmp };
            rewrite_file.append(std::as_bytes(std::span{rewrite_buffer_}));
            rewrite_file.flush();
            rewrite_file.close();
            rewrite_buffer_.clear();
        }

        if (!writer_.replace_from(tmp)) {
            SPDLOG_ERROR("AOF swap rewrite file failed");
            return;
        }

        last_rewrite_size_.store(size(), std::memory_order_relaxed);
    };

    boost::asio::post(io_context_, task);
}

AOF::AofRewriter::~AofRewriter()
{
    stop();
}

auto AOF::AofRewriter::try_start(std::span<const std::unique_ptr<DBShard>> shards,
                                 std::filesystem::path tmp_path,
                                 OnComplete on_complete) -> bool
{
    bool expected = false;
    if (!is_rewriting_.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        return false;

    if (worker_.joinable())
        worker_.join();

    worker_ = std::jthread([this, shards, 
                            tmp_path = std::move(tmp_path),
                            on_complete = std::move(on_complete)](const std::stop_token& stoken) mutable
    {
        SPDLOG_INFO("AOF: Starting background rewrite...");

        try {
            AofCommandEncoder::dump_shards_to_file(shards, tmp_path);

            if (!stoken.stop_requested() && on_complete)
                on_complete(tmp_path);

            SPDLOG_INFO("AOF: rewrite completed.");
        }
        catch (const std::exception& ex) {
            SPDLOG_ERROR("AOF rewrite failed: {}", ex.what());
            std::error_code ec;
            fs::remove(tmp_path, ec);
        }
        catch (...) {
            SPDLOG_ERROR("AOF rewrite failed: unknown error");
            std::error_code ec;
            fs::remove(tmp_path, ec);
        }

        is_rewriting_.store(false, std::memory_order_relaxed);
    });

    return true;
}

void AOF::AofRewriter::stop()
{
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }

    is_rewriting_.store(false, std::memory_order_relaxed);
}

AOF::AofWriter::AofWriter(std::filesystem::path path)
  : path_{ std::move(path) }
{
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1) {
        SPDLOG_CRITICAL("Failed to open AOF file: {}", path_.string());
        healthy_.store(false, std::memory_order_relaxed);
    }
}

AOF::AofWriter::~AofWriter()
{
    stop();
}

auto AOF::AofWriter::append(std::string_view message) -> bool
{
    std::lock_guard<std::mutex> lock{ mutex_ };

    if (stopping_.load(std::memory_order_relaxed))
        return false;

    if (fd_ == -1)
        return false;

    auto* data = message.data();
    auto remaining = message.size();

    while (remaining > 0) {
        auto written = ::write(fd_, data, remaining);
        if (written > 0) {
            data += written;
            remaining -= static_cast<size_t>(written);
            continue;
        }

        if (written == -1 && errno == EINTR)
            continue;

        if (is_healthy()) {
            const auto err = errno;
            SPDLOG_CRITICAL("AOF Write Error: errno={}, message={}", err,
                            std::generic_category().message(err));
            healthy_.store(false, std::memory_order_relaxed);
        }

        return false;
    }

    return true;
}

auto AOF::AofWriter::sync() -> bool
{
    std::lock_guard<std::mutex> lock{ mutex_ };

    if (fd_ == -1)
        return false;

    if (::fdatasync(fd_) == -1) {
        if (is_healthy()) {
            const auto err = errno;
            SPDLOG_CRITICAL("AOF fdatasync Error: errno={}, message={}", err,
                            std::generic_category().message(err));
            healthy_.store(false, std::memory_order_relaxed);
        }
        return false;
    }

    if (!is_healthy()) {
        SPDLOG_INFO("AOF is recovered.");
        healthy_.store(true, std::memory_order_relaxed);
    }

    return true;
}

void AOF::AofWriter::close()
{
    std::lock_guard<std::mutex> lock{ mutex_ };

    if (fd_ == -1)
        return;

    if (::close(fd_) == -1) {
        const auto err = errno;
        SPDLOG_ERROR("AOF writer close failed: errno={}, message={}", err,
                     std::generic_category().message(err));
    }

    fd_ = -1;
}

void AOF::AofWriter::reopen(std::filesystem::path path)
{
    std::lock_guard<std::mutex> lock{ mutex_ };

    if (fd_ != -1 && ::close(fd_) == -1) {
        const auto err = errno;
        SPDLOG_ERROR("AOF writer close failed: errno={}, message={}", err,
                     std::generic_category().message(err));
    }

    fd_ = -1;
    path_ = std::move(path);
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1) {
        const auto err = errno;
        SPDLOG_CRITICAL("Failed to reopen AOF file: {} (errno={}, message={})",
                        path_.string(), err, std::generic_category().message(err));
        healthy_.store(false, std::memory_order_relaxed);
    }
}

auto AOF::AofWriter::reset_file(std::string_view backup_suffix) -> bool
{
    std::lock_guard<std::mutex> lock{ mutex_ };

    if (fd_ != -1 && ::fdatasync(fd_) == -1) {
        const auto err = errno;
        SPDLOG_ERROR("AOF writer fdatasync failed before reset: errno={}, message={}",
                     err, std::generic_category().message(err));
        return false;
    }

    if (fd_ != -1 && ::close(fd_) == -1) {
        const auto err = errno;
        SPDLOG_ERROR("AOF writer close failed before reset: errno={}, message={}",
                     err, std::generic_category().message(err));
        return false;
    }
    fd_ = -1;

    auto backup_path = path_;
    backup_path += backup_suffix;

    std::error_code ec;
    fs::rename(path_, backup_path, ec);
    if (ec) {
        SPDLOG_ERROR("AOF writer rename to backup failed: {}", ec.message());
        return false;
    }

    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1) {
        const auto err = errno;
        SPDLOG_CRITICAL("AOF writer reopen failed after reset: errno={}, message={}",
                        err, std::generic_category().message(err));
        healthy_.store(false, std::memory_order_relaxed);
        return false;
    }

    fs::remove(backup_path, ec);
    if (ec) {
        SPDLOG_WARN("AOF writer remove backup file failed: {}", ec.message());
    }

    return true;
}

auto AOF::AofWriter::replace_from(const std::filesystem::path& tmp_path) -> bool
{
    std::lock_guard<std::mutex> lock{ mutex_ };

    if (fd_ != -1 && ::fdatasync(fd_) == -1) {
        const auto err = errno;
        SPDLOG_ERROR("AOF writer fdatasync failed before replace: errno={}, message={}",
                     err, std::generic_category().message(err));
        return false;
    }

    if (fd_ != -1 && ::close(fd_) == -1) {
        const auto err = errno;
        SPDLOG_ERROR("AOF writer close failed before replace: errno={}, message={}",
                     err, std::generic_category().message(err));
        return false;
    }
    fd_ = -1;

    std::error_code ec;
    fs::rename(tmp_path, path_, ec);
    if (ec) {
        SPDLOG_ERROR("AOF writer replace rename failed: {}", ec.message());
        return false;
    }

    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1) {
        const auto err = errno;
        SPDLOG_CRITICAL("AOF writer reopen failed after replace: errno={}, message={}",
                        err, std::generic_category().message(err));
        healthy_.store(false, std::memory_order_relaxed);
        return false;
    }

    return true;
}

void AOF::AofWriter::stop()
{
    stopping_.store(true, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock{ mutex_ };
    if (fd_ != -1) {
        if (::fdatasync(fd_) == -1) {
            const auto err = errno;
            SPDLOG_ERROR("AOF writer fdatasync failed on shutdown: errno={}, message={}", err,
                         std::generic_category().message(err));
        }

        if (::close(fd_) == -1) {
            const auto err = errno;
            SPDLOG_ERROR("AOF writer close failed: errno={}, message={}", err,
                         std::generic_category().message(err));
        }

        fd_ = -1;
    }
}

} // namespace spg::redis
