#ifndef SPONGE_HTTP_SERVER_H
#define SPONGE_HTTP_SERVER_H

#include <functional>
#include <optional>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <ctre.hpp>

#include "method.h"
#include "path_pattern.h"
#include "request.h"
#include "router.h"

namespace spg::http {

class Server {
private:
    using BeastResponse = boost::beast::http::response<boost::beast::http::string_body>;
    using HttpResponse = std::optional<BeastResponse>;
    using Handler = std::function<std::optional<HttpResponse>(const Request& request)>;

public:
    Server(std::string_view address, std::string_view port, int threads = 1);

    template<typename Func>
    void Use(Func&& middleware)
    {
        router_.Use(std::forward<Func>(middleware));
    }

    template<Method method, ctll::fixed_string pattern, typename Func>
    void Map(Func&& handler)
    {
        router_.Map<method, path_pattern_to_regex<pattern>()>(std::forward<Func>(handler));
    }

    template<ctll::fixed_string pattern, typename Func>
    void Get(Func&& handler)
    {
        Map<Method::Get, pattern>(std::forward<Func>(handler));
    }

    template<ctll::fixed_string pattern, typename Func>
    void Post(Func&& handler)
    {
        Map<Method::Post, pattern>(std::forward<Func>(handler));
    }

    template<ctll::fixed_string pattern, typename Func>
    void Put(Func&& handler)
    {
        Map<Method::Put, pattern>(std::forward<Func>(handler));
    }

    template<ctll::fixed_string pattern, typename Func>
    void Delete(Func&& handler)
    {
        Map<Method::Delete, pattern>(std::forward<Func>(handler));
    }

    void run();

private:
    std::string address_;
    std::string port_;
    boost::asio::io_context io_context_;
    Router router_;

    auto do_session(boost::asio::ip::tcp::socket socket) -> boost::asio::awaitable<void>;

    auto listener() -> boost::asio::awaitable<void>;
};

} // namespace spg::http

#endif // SPONGE_HTTP_SERVER_H
