#include <sponge/redis/server.h>

#include <algorithm>
#include <print>

#include <sys/socket.h>

#include <boost/asio.hpp>

#include "session.h"

namespace asio = boost::asio;

namespace spg::redis {

Server::Server(std::string_view address, std::string_view port, size_t threads)
  : application_context_{ threads },
    address_{ address },
    port_{ port }
{
    acceptors_.reserve(threads);
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
                       listener(acceptors_.back(), application_context_.thread_context(i)), 
                       asio::detached);
    }

    auto& first_context = application_context_.io_context(0);
    asio::co_spawn(first_context, graceful_shutdown(first_context), asio::detached);

    application_context_.run();
}

auto Server::listener(boost::asio::ip::tcp::acceptor& acceptor, ThreadContext context) -> boost::asio::awaitable<void>
{
    auto address = acceptor.local_endpoint().address().to_string();
    auto port = acceptor.local_endpoint().port();
    std::print("Listener started on {}:{}\n", address, port);

    try {
        while (true) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            asio::co_spawn(acceptor.get_executor(), do_session(std::move(socket), context), asio::detached);
        }
    } catch (const boost::system::system_error& e) {
        if (!stopping_)
            std::print("error occurred: {}\n", e.what());

        std::print("Listener stopped. Ready to shutdown\n");
    }
}

auto Server::do_session(asio::ip::tcp::socket socket, ThreadContext& context) -> boost::asio::awaitable<void>
{
    Session session{ std::move(socket), context };
    co_await session.run();
}

auto Server::graceful_shutdown(boost::asio::io_context& context) -> boost::asio::awaitable<void>
{
    asio::signal_set signals{ context, SIGINT, SIGTERM };
    co_await signals.async_wait(asio::use_awaitable);
    std::print("Shutting down...\n");

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

} // namespace spg::redis
