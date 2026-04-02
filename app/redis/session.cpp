#include "session.h"

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
    CommandContext context{ 
        .application_context = context_, 
        .reply = reply_,
        .is_aof_loading = false
    };
    
    context.aof_buffer.reserve(BUFFER_SIZE);

    try {
        while (true) {
            auto mutable_buffer = buffer.prepare(BUFFER_SIZE);
            auto n = co_await socket_.async_read_some(mutable_buffer, asio::use_awaitable);
            buffer.commit(n);

            while (buffer.size() > 0) {
                std::string_view data{ static_cast<const char*>(buffer.data().data()), buffer.size() };

                auto [commands, consumed_bytes] = resp::parse_request(data);

                if (commands.empty())
                    break; // 半包，继续等待数据

                for (const auto& cmd : commands)
                    commands::dispatch(context, cmd);

                buffer.consume(consumed_bytes);
            }

            // 批量写入 AOF，减少磁盘 I/O 次数
            if (!context.aof_buffer.empty()) {
                context_.aof().append(std::move(context.aof_buffer));
                context.aof_buffer = std::pmr::string{};
                context.aof_buffer.reserve(BUFFER_SIZE);
            }

            if (!reply_.empty()) {
                co_await asio::async_write(socket_, reply_.view_buffer(), asio::use_awaitable);
                reply_.clear();
            }
        }
    }
    catch (const boost::system::system_error& e) {
        if (e.code() != asio::error::eof && e.code() != asio::error::connection_reset)
            SPDLOG_ERROR("Session error: {}", e.what());

        // 在连接关闭时，确保将未写入 AOF 的命令写入磁盘，减少数据丢失的可能性
        if (!context.aof_buffer.empty())
            context_.aof().append(std::move(context.aof_buffer));
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Session exception: {}", e.what());

        // 在连接关闭时，确保将未写入 AOF 的命令写入磁盘，减少数据丢失的可能性
        if (!context.aof_buffer.empty())
            context_.aof().append(std::move(context.aof_buffer));
    }
}

} // namespace spg::redis
