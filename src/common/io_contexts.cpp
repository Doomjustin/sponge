#include <sponge/io_contexts.h>

#include <algorithm>
#include <cassert>
#include <memory>

namespace asio = boost::asio;

namespace spg {

IOContexts::IOContexts(size_t size)
{
    assert(size > 0);

    io_contexts_.reserve(size);
    threads_.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        auto context = std::make_shared<asio::io_context>();
        io_contexts_.push_back(context);
        works_.push_back(asio::make_work_guard(*context));
        threads_.emplace_back([context = io_contexts_.back()] { context->run(); });
    }
}

IOContexts::~IOContexts()
{
    std::ranges::for_each(io_contexts_, [](auto& ctx) -> void { ctx->stop(); });
    std::ranges::for_each(threads_, [](auto& thread) { if (thread.joinable()) thread.join(); });
}

} // namespace spg
