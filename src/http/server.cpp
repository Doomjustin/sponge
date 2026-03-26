#include <sponge/http/server.h>

#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>

#include <sponge/http/middleware.h>
#include <sponge/http/response.h>
#include <sponge/http/session.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace spg::http {

Server::Server(std::string_view address, std::string_view port, int threads)
  : address_{ address }, 
    port_{ port }, 
    io_context_{ threads }, 
    router_{}
{
    // Default middleware assembly order (outer -> inner):
    // access_log -> request_id -> recover -> routes
    Use(middleware::access_log());
    Use(middleware::request_id());
    Use(middleware::recover());
}

void Server::run()
{
    asio::co_spawn(io_context_, listener(), asio::detached);
    io_context_.run();
}

auto Server::do_session(boost::asio::ip::tcp::socket socket) -> boost::asio::awaitable<void>
{
    Session session{ std::move(socket), router_ };
    co_await session.session();
}

auto Server::listener() -> asio::awaitable<void>
{
    using Acceptor = asio::ip::tcp::acceptor;
    using Endpoint = asio::ip::tcp::endpoint;
    using Resolver = asio::ip::tcp::resolver;

    Resolver resolver{ io_context_ };
    Endpoint endpoint = *resolver.resolve(address_, port_).begin();

    Acceptor acceptor{ io_context_ };
    acceptor.open(endpoint.protocol());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen();

    while (true) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        co_spawn(io_context_, do_session(std::move(socket)), asio::detached);
    }
}

} // namespace spg::http
