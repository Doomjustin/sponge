#include "session.h"

#include <boost/asio.hpp>

namespace asio = boost::asio;

namespace spg::redis {

Session::Session(Socket socket)
  : socket_{ std::move(socket) }
{}

auto Session::run() -> boost::asio::awaitable<void>
{
    co_await asio::async_write(socket_, asio::buffer("Hello, World!\n"), asio::use_awaitable);
}

} // namespace spg::redis
