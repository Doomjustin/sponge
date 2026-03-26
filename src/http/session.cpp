#include <sponge/http/session.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>

namespace beast = boost::beast;
namespace asio = boost::asio;

namespace spg::http {

namespace {

auto method_cast(beast::http::verb verb) -> Method
{
    switch (verb) {
    case beast::http::verb::get:
        return Method::Get;
    case beast::http::verb::post:
        return Method::Post;
    case beast::http::verb::put:
        return Method::Put;
    case beast::http::verb::delete_:
        return Method::Delete;
    case beast::http::verb::patch:
        return Method::Patch;
    case beast::http::verb::options:
        return Method::Options;
    default:
        return Method::None;
    }
}

} // namespace

Session::Session(boost::asio::ip::tcp::socket socket, Router& router)
  : stream_{ std::move(socket) }, 
    router_{ router }
{
}

auto Session::session() -> boost::asio::awaitable<void>
{
    beast::flat_buffer buffer;

    try {
        while (true) {
            using namespace std::literals;
            stream_.expires_after(30s);

            beast::http::request<beast::http::string_body> request{};
            co_await beast::http::async_read(stream_, buffer, request, asio::use_awaitable);

            RequestBuilder request_builder{};
            request_builder.method(method_cast(request.method()));
            request_builder.path(request.target());
            request_builder.body(request.body());
            request_builder.version(request.version());
            request_builder.keep_alive(request.keep_alive());

            for (const auto& field : request)
                request_builder.add_header(field.name_string(), field.value());

            auto http_request = request_builder.build();
            auto handle_result = router_.dispatch(http_request);

            if (handle_result)
                co_await beast::http::async_write(stream_, *handle_result, asio::use_awaitable);
            else
                co_await return_error(http_request, handle_result.error());

            if (!request.keep_alive()) {
                stream_.socket().shutdown(asio::ip::tcp::socket::shutdown_both);
                break;
            }
        }
    }
    catch (const beast::system_error& e) {
        if (e.code() != beast::http::error::end_of_stream &&
            e.code() != asio::error::operation_aborted)
            SPDLOG_ERROR("Session error: {}", e.what());
    }
}

auto Session::return_error(const Request& request, Status status) -> boost::asio::awaitable<void>
{
    beast::http::response<beast::http::string_body> response{
        static_cast<beast::http::status>(status), request.version()
    };
    response.set(beast::http::field::content_type, templates::CONTENT_TEXT);
    response.body() = templates::error(request, status);
    response.keep_alive(request.keep_alive());

    response.prepare_payload();
    co_await beast::http::async_write(stream_, response, asio::use_awaitable);
}

} // namespace spg::http
