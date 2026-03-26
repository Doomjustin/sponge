#ifndef SPONGE_IO_CONTEXTS_H
#define SPONGE_IO_CONTEXTS_H

#include <cstddef>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/executor_work_guard.hpp>

namespace spg {

class IOContexts {
public:
    using Context = boost::asio::io_context;
    using ContextPtr = std::shared_ptr<Context>;
    using WorkGuard = boost::asio::executor_work_guard<Context::executor_type>;
    using Size = std::size_t;

    explicit IOContexts(Size size);

    IOContexts(const IOContexts&) = delete;
    auto operator=(const IOContexts&) = delete;

    ~IOContexts();

    void run();

    void stop();

    void force_stop();

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return io_contexts_.size();
    }

    auto operator[](Size index) -> Context& { return *io_contexts_[index]; }

private:
    std::vector<ContextPtr> io_contexts_;
    std::vector<std::jthread> threads_;
    std::list<WorkGuard> works_;
    Size next_io_context_ = 0;

    void join_threads();
};

} // namespace spg

#endif // SPONGE_IO_CONTEXTS_H
