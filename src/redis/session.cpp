#include "session.h"

#include <print>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "commands.h"
#include "protocol.h"

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace spg::redis {

Session::Session(Socket socket, ApplicationContext& context, Index index)
  : socket_{ std::move(socket) }, 
    context_{ context },
    index_{ index }
{
    socket_.set_option(asio::ip::tcp::no_delay(true));
}

auto Session::run() -> boost::asio::awaitable<void>
{
    beast::flat_buffer buffer{ MAX_QUERY_SIZE };
    CommandContext context{ .application_context = context_, .reply = reply_ };

    try {
        while (true) {
            auto mutable_buffer = buffer.prepare(BUFFER_SIZE);
            auto n = co_await socket_.async_read_some(mutable_buffer, asio::use_awaitable);
            buffer.commit(n);

            while (buffer.size() > 0) {
                std::string_view data{ static_cast<const char*>(buffer.data().data()), buffer.size() };

                auto [commands, consumed_bytes] = resp::parse_request(data, context_.resource(index_));

                if (commands.empty())
                    break; // 半包，继续等待数据

                for (const auto& cmd : commands)
                    commands::dispatch(context, cmd);

                buffer.consume(consumed_bytes);
            }

            if (!reply_.empty()) {
                co_await asio::async_write(socket_, reply_.view_buffer(), asio::use_awaitable);
                reply_.clear();
            }
        }
    }
    catch (const boost::system::system_error& e) {
        if (e.code() != asio::error::eof && e.code() != asio::error::connection_reset)
            std::print("Session error: {}\n", e.what());
    }
    catch (const std::exception& e) {
        std::print("Session exception: {}\n", e.what());
    }
}

} // namespace spg::redis
