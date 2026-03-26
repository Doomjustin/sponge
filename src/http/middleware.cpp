#include <sponge/http/middleware.h>

#include <atomic>
#include <chrono>
#include <string>

#include <spdlog/spdlog.h>

#include <sponge/http/exceptions.h>

namespace spg::http {

auto middleware::request_id(std::string_view header_name) -> Middleware
{
    auto header = std::string{ header_name };

    return [header = std::move(header)](const Request& request, const Next& next) -> DispatchResult 
    {
        static std::atomic_uint64_t request_seq{ 0 };

        auto request_id_value = std::string{};
        if (auto incoming_id = request.get_header(header); incoming_id)
            request_id_value = std::string{ *incoming_id };
        else
            request_id_value ="req-" + std::to_string(request_seq.fetch_add(1, std::memory_order_relaxed) + 1);

        auto result = next(request);
        if (result)
            result->set(header, request_id_value);

        return result;
    };
}

auto middleware::access_log(std::string_view request_id_header) -> Middleware
{
    auto request_id_key = std::string{ request_id_header };

    return [request_id_key = std::move(request_id_key)](const Request& request, const Next& next) -> DispatchResult 
    {
        using namespace std::chrono;

        const auto start = steady_clock::now();
        auto result = next(request);

        const auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);

        auto request_id_value = std::string{ "-" };
        if (result) {
            if (auto it = result->find(request_id_key); it != result->end())
                request_id_value = std::string{ it->value() };

            SPDLOG_INFO("[{}] {} {} -> {} ({} us)", 
                        request_id_value, 
                        request.method(),
                        request.path(), 
                        result->result_int(), 
                        elapsed.count());
        }
        else {
            if (auto incoming_id = request.get_header(request_id_key); incoming_id)
                request_id_value = std::string{ *incoming_id };

            SPDLOG_WARN("[{}] {} {} -> {} ({} us)", 
                        request_id_value, 
                        request.method(),
                        request.path(), 
                        static_cast<int>(result.error()), 
                        elapsed.count());
        }

        return result;
    };
}

auto middleware::recover() -> Middleware
{
    return [](const Request& request, const Next& next) -> DispatchResult
    {
        try {
            return next(request);
        }
        catch (const BadRequestException&) {
            return std::unexpected(Status::BadRequest);
        }
        catch (const std::exception& e) {
            SPDLOG_ERROR("Unhandled exception in middleware chain: {}", e.what());
            return std::unexpected(Status::InternalServerError);
        }
        catch (...) {
            SPDLOG_ERROR("Unhandled non-std exception in middleware chain");
            return std::unexpected(Status::InternalServerError);
        }
    };
}

} // namespace spg::http
