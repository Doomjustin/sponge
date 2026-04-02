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
    explicit IOContexts(size_t size);

    IOContexts(const IOContexts&) = delete;
    auto operator=(const IOContexts&) = delete;

    ~IOContexts();

    [[nodiscard]]
    constexpr auto size() const noexcept -> size_t
    {
        return io_contexts_.size();
    }

    auto operator[](size_t index) -> boost::asio::io_context& 
    { 
        return *io_contexts_[index]; 
    }

private:
    using Context = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<Context::executor_type>;

    std::pmr::vector<std::shared_ptr<Context>> io_contexts_;
    std::pmr::vector<std::jthread> threads_;
    std::pmr::list<WorkGuard> works_;
};

} // namespace spg

#endif // SPONGE_IO_CONTEXTS_H
