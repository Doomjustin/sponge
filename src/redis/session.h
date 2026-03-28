#ifndef SPONGE_REDIS_SESSION_H
#define SPONGE_REDIS_SESSION_H

#include <array>

#include <boost/asio.hpp>

#include <sponge/redis/application_context.h>

#include "reply.h"

namespace spg::redis {

class Session {
public:
    using Index = size_t;
    using Size = size_t;
    using Socket = boost::asio::ip::tcp::socket;

    Session(Socket socket, ThreadContext& context);

    auto run() -> boost::asio::awaitable<void>;

private:
    static constexpr size_t BUFFER_SIZE = 8192;
    static constexpr size_t MAX_QUERY_SIZE = 1024 * 1024 * 1024; // 1GB

    Socket socket_;
    ThreadContext& context_;
    Reply reply_;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_SESSION_H
