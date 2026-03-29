#ifndef SPONGE_REDIS_SERVER_H
#define SPONGE_REDIS_SERVER_H

#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>

#include <boost/asio.hpp>

#include "application_context.h"

namespace spg::redis {

class Server {
public:
    Server(std::string_view address, std::string_view port, size_t threads);

    void run();

private:
    static constexpr std::string_view AOF_FILENAME = "/tmp/sponge/redis.aof";

    ApplicationContext application_context_;
    std::pmr::string address_;
    std::pmr::string port_;
    std::atomic<bool> stopping_ = false;
    std::pmr::vector<boost::asio::ip::tcp::acceptor> acceptors_;

    auto listener(boost::asio::ip::tcp::acceptor& acceptor, size_t index) -> boost::asio::awaitable<void>;

    auto do_session(boost::asio::ip::tcp::socket socket, size_t index) -> boost::asio::awaitable<void>;

    auto graceful_shutdown(boost::asio::io_context& context) -> boost::asio::awaitable<void>;

    void load_aof(std::string_view filepath);
};

} // namespace spg::redis

#endif // SPONGE_REDIS_SERVER_H
