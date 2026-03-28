#include <sponge/io_contexts.h>

#include <algorithm>
#include <cassert>
#include <memory>

namespace asio = boost::asio;

namespace spg {

IOContexts::IOContexts(Size size)
{
    assert(size > 0);

    for (Size i = 0; i < size; ++i) {
        auto context = std::make_shared<asio::io_context>();
        io_contexts_.push_back(context);
        works_.push_back(asio::make_work_guard(*context));
    }
}

IOContexts::~IOContexts()
{
    force_stop();
    join_threads();
}

void IOContexts::run()
{
    for (size_t i = 1; i < io_contexts_.size(); ++i)
        threads_.emplace_back([this, i] () { io_contexts_[i]->run(); });

    io_contexts_[0]->run();

    join_threads();
}

void IOContexts::stop()
{
    std::ranges::for_each(works_, [](auto& work) { work.reset(); });
    std::ranges::for_each(io_contexts_, [](auto& ctx) -> void { ctx->stop(); });
}

void IOContexts::force_stop()
{
    std::ranges::for_each(io_contexts_, [](auto& ctx) -> void { ctx->stop(); });
}

void IOContexts::join_threads()
{
    std::ranges::for_each(threads_, [](auto& thread) { if (thread.joinable()) thread.join(); });
}

} // namespace spg
