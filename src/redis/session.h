#ifndef SPONGE_REDIS_SESSION_H
#define SPONGE_REDIS_SESSION_H

#include <array>

#include <boost/asio.hpp>

namespace spg::redis {

class Session {
public:
    using Index = std::size_t;
    using Size = std::size_t;
    using Socket = boost::asio::ip::tcp::socket;

    explicit Session(Socket socket);

    auto run() -> boost::asio::awaitable<void>;

private:
    static constexpr std::size_t BUFFER_SIZE = 8192;

    Socket socket_;
    std::array<char, BUFFER_SIZE> buffer_{};
    Size write_idx_ = 0;
    Index index_ = 0;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_SESSION_H
