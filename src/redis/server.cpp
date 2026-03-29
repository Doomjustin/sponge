#include <sponge/redis/server.h>

#include <algorithm>
#include <array>
#include <fstream>

#include <sys/socket.h>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include "command_context.h"
#include "commands.h"
#include "protocol.h"
#include "reply.h"
#include "session.h"

namespace asio = boost::asio;

namespace spg::redis {

Server::Server(std::string_view address, std::string_view port, size_t threads)
  : application_context_{ threads, AOF_FILENAME },
    address_{ address },
    port_{ port }
{
    acceptors_.reserve(threads);
    load_aof(AOF_FILENAME);
}

void Server::run() 
{
    using reuse_port = asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT>;

    asio::ip::tcp::resolver resolver{ io_context_ };
    asio::ip::tcp::endpoint endpoint = *resolver.resolve(address_, port_).begin();

    for (size_t i = 0; i < application_context_.size(); ++i) {
        auto& context = application_context_.io_context(i);
        asio::ip::tcp::acceptor acceptor{ context };
        acceptor.open(endpoint.protocol());
        acceptor.set_option(asio::socket_base::reuse_address(true));
        acceptor.set_option(reuse_port(true));
        acceptor.bind(endpoint);
        acceptor.listen();
        acceptors_.push_back(std::move(acceptor));

        asio::co_spawn(context, listener(i), asio::detached);
    }

    asio::co_spawn(io_context_, graceful_shutdown(), asio::detached);
    asio::co_spawn(io_context_, cron(), asio::detached);
    io_context_.run();
}

auto Server::listener(size_t index) -> boost::asio::awaitable<void>
{
    auto& acceptor = acceptors_[index];
    auto address = acceptor.local_endpoint().address().to_string();
    auto port = acceptor.local_endpoint().port();
    SPDLOG_INFO("Listening on {}:{}", address, port);

    try {
        while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(acceptor.get_executor(), do_session(std::move(socket), index), asio::detached);
        }
    } catch (const boost::system::system_error& e) {
        if (!stopping_.load(std::memory_order_relaxed))
           SPDLOG_ERROR("error occurred: {}", e.what());

        SPDLOG_INFO("Listener stopped. Ready to shutdown");
    }
}

auto Server::do_session(asio::ip::tcp::socket socket, size_t index) -> boost::asio::awaitable<void>
{
    Session session{ std::move(socket), application_context_, index };
    co_await session.run();
}

auto Server::graceful_shutdown() -> boost::asio::awaitable<void>
{
    asio::signal_set signals{ io_context_, SIGINT, SIGTERM };
    co_await signals.async_wait(asio::use_awaitable);
    
    stopping_.store(true, std::memory_order_relaxed);
    
    SPDLOG_INFO("Shutting down...");

    auto stop_acceptors = [] (auto& acceptor) 
    {
        boost::system::error_code ec;
        acceptor.cancel(ec);
        acceptor.close(ec);
    };

    std::ranges::for_each(acceptors_, stop_acceptors);
    io_context_.stop();
}

void Server::load_aof(std::string_view filepath)
{
    std::ifstream file{ filepath.data(), std::ios::binary };
    if (!file) {
        SPDLOG_INFO("No AOF file found at '{}', starting with an empty dataset.", filepath);
        return;
    }

    SPDLOG_INFO("Loading AOF file from '{}'", filepath);

    Reply reply;
    CommandContext fake_context {
        .application_context = application_context_,
        .reply = reply,
        .is_aof_loading = true
    };

    std::string buffer{};
    buffer.reserve(1024 * 1024 * 4);
    std::array<char, 65536> chunk;

    size_t count = 0;
    while (file.read(chunk.data(), chunk.size()) || file.gcount() > 0) {
        buffer.append(chunk.data(), file.gcount());

        while (!buffer.empty()) {
            auto [commands, consumed_bytes] = resp::parse_request(buffer);

            if (commands.empty())
                break; // 半包，继续读取数据

            for (const auto& cmd : commands) {
                commands::dispatch(fake_context, cmd);
                fake_context.reply.clear(); // 加载 AOF 时不需要回复客户端
                ++count;
            }

            buffer.erase(0, consumed_bytes);
        }
    }

    SPDLOG_INFO("AOF Loading complete! Successfully replayed {} commands.", count);
}

auto Server::cron() -> boost::asio::awaitable<void>
{
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer{ executor };

    while (true) {
        using namespace std::chrono_literals;
        timer.expires_after(1s);

        try {
            co_await timer.async_wait(asio::use_awaitable);
        }
        catch (const boost::system::system_error& e) {
            if (e.code() != asio::error::operation_aborted)
                SPDLOG_ERROR("Cron timer error: {}", e.what());
            break;
        }

        auto& aof = application_context_.aof();
        if (!aof.is_healthy() || aof.is_rewriting())
            continue; // 如果 AOF 不健康或正在重写，等待重写完成后再检查状态

        static constexpr size_t AOF_REWRITE_SIZE_THRESHOLD = 1024 * 1024 * 64; // 64 MB
        if (aof.size() >= AOF_REWRITE_SIZE_THRESHOLD) {
            auto last_size = aof.last_rewrite_size();
            auto growth_rat = last_size == 0 ? 100.0 : static_cast<double>(aof.size() - last_size) / last_size;
            
            if (growth_rat >= 1.0) {
                SPDLOG_INFO("AOF Auto-triggering rewrite! Growth: {:.2f}%", growth_rat * 100);
                aof.background_rewrite(application_context_.shards());
                last_size = aof.size();
            }
        }
    }

    SPDLOG_INFO("Cron task stopped.");
}

} // namespace spg::redis
