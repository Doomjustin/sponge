#ifndef SPONGE_HTTP_SESSION_H
#define SPONGE_HTTP_SESSION_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "request.h"
#include "router.h"

namespace spg::http {

class Session {
public:
    Session(boost::asio::ip::tcp::socket socket, Router& router);

    auto session() -> boost::asio::awaitable<void>;

private:
    boost::beast::tcp_stream stream_;
    Router& router_;

    auto return_error(const Request& request, Status status) -> boost::asio::awaitable<void>;
};

} // namespace spg::http

#endif // SPONGE_HTTP_SESSION_H
