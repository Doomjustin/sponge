#ifndef SPONGE_REDIS_SERVER_H
#define SPONGE_REDIS_SERVER_H

#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio.hpp>

#include "application_context.h"

namespace spg::redis {

class Server {
public:
    Server(std::string_view address, std::string_view port, size_t threads);

    void run();

private:
    ApplicationContext application_context_;
    std::string address_;
    std::string port_;
    std::atomic<bool> stopping_ = false;
    std::vector<boost::asio::ip::tcp::acceptor> acceptors_;

    auto listener(boost::asio::ip::tcp::acceptor& acceptor, size_t index) -> boost::asio::awaitable<void>;

    auto do_session(boost::asio::ip::tcp::socket socket, size_t index) -> boost::asio::awaitable<void>;

    auto graceful_shutdown(boost::asio::io_context& context) -> boost::asio::awaitable<void>;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_SERVER_H
