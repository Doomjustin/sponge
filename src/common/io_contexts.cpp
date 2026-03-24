#include <sponge/io_contexts.h>

#include <algorithm>
#include <cassert>
#include <memory>

namespace asio = boost::asio;

namespace spg {

IOContextPool::IOContextPool(Size size)
{
    assert(size > 0);

    for (Size i = 0; i < size; ++i) {
        auto context = std::make_shared<asio::io_context>();
        io_contexts_.push_back(context);
        works_.push_back(asio::make_work_guard(*context));
        threads_.emplace_back([context]() { context->run(); });
    }
}

IOContextPool::~IOContextPool()
{
    stop();
    std::ranges::for_each(threads_, [](auto& thread) { thread.join(); });
}

void IOContextPool::stop()
{
    auto stop_context = [](auto& ctx) -> void { ctx->stop(); };
    std::ranges::for_each(io_contexts_, stop_context);
}

} // namespace spg
