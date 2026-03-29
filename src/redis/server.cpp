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

    asio::ip::tcp::resolver resolver{ application_context_.io_context(0) };
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

        asio::co_spawn(context, 
                       listener(acceptors_.back(), i), 
                       asio::detached);
    }

    auto& first_context = application_context_.io_context(0);
    asio::co_spawn(first_context, graceful_shutdown(first_context), asio::detached);

    application_context_.run();
}

auto Server::listener(boost::asio::ip::tcp::acceptor& acceptor, size_t index) -> boost::asio::awaitable<void>
{
    auto address = acceptor.local_endpoint().address().to_string();
    auto port = acceptor.local_endpoint().port();
    SPDLOG_INFO("Listening on {}:{}", address, port);

    try {
        while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(acceptor.get_executor(), do_session(std::move(socket), index), asio::detached);
        }
    } catch (const boost::system::system_error& e) {
        if (!stopping_)
           SPDLOG_ERROR("error occurred: {}", e.what());

        SPDLOG_INFO("Listener stopped. Ready to shutdown");
    }
}

auto Server::do_session(asio::ip::tcp::socket socket, size_t index) -> boost::asio::awaitable<void>
{
    Session session{ std::move(socket), application_context_, index };
    co_await session.run();
}

auto Server::graceful_shutdown(boost::asio::io_context& context) -> boost::asio::awaitable<void>
{
    asio::signal_set signals{ context, SIGINT, SIGTERM };
    co_await signals.async_wait(asio::use_awaitable);
    
    SPDLOG_INFO("Shutting down...");

    boost::system::error_code ec;

    auto stop_acceptors = [] (auto& acceptor) 
    {
        boost::system::error_code ec;
        acceptor.cancel(ec);
        acceptor.close(ec);
    };

    // 只会有一个线程在这里执行，所以不需要担心竞争条件
    stopping_ = true;
    std::ranges::for_each(acceptors_, stop_acceptors);

    application_context_.stop();
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

} // namespace spg::redis
