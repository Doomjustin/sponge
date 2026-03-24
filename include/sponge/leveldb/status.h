#ifndef SPONGE_LEVELDB_STATUS_H
#define SPONGE_LEVELDB_STATUS_H

#include <cstdint>
#include <format>
#include <string_view>

namespace spg::leveldb {

class Status {
public:
    // 默认构造函数创建一个 OK 状态
    Status() = default;

    [[nodiscard]]
    auto message() const -> std::string_view
    {
        return message_;
    }

    [[nodiscard]]
    constexpr auto is_ok() const -> bool
    {
        return code_ == Code::OK;
    }

    [[nodiscard]]
    constexpr auto is_not_found() const -> bool
    {
        return code_ == Code::NotFound;
    }

    [[nodiscard]]
    constexpr auto is_corruption() const -> bool
    {
        return code_ == Code::Corruption;
    }

    [[nodiscard]]
    constexpr auto is_not_supported() const -> bool
    {
        return code_ == Code::NotSupported;
    }

    [[nodiscard]]
    constexpr auto is_invalid_argument() const -> bool
    {
        return code_ == Code::InvalidArgument;
    }

    [[nodiscard]]
    constexpr auto is_io_error() const -> bool
    {
        return code_ == Code::IOError;
    }

    static auto ok() -> Status { return Status{}; }

    template<typename... Args>
    static auto not_found(std::format_string<Args...> fmt, Args&&... args) -> Status
    {
        return Status{ Code::NotFound, std::format(fmt, std::forward<Args>(args)...) };
    }

    template<typename... Args>
    static auto corruption(std::format_string<Args...> fmt, Args&&... args) -> Status
    {
        return Status{ Code::Corruption, std::format(fmt, std::forward<Args>(args)...) };
    }

    template<typename... Args>
    static auto not_supported(std::format_string<Args...> fmt, Args&&... args) -> Status
    {
        return Status{ Code::NotSupported, std::format(fmt, std::forward<Args>(args)...) };
    }

    template<typename... Args>
    static auto invalid_argument(std::format_string<Args...> fmt, Args&&... args) -> Status
    {
        return Status{ Code::InvalidArgument, std::format(fmt, std::forward<Args>(args)...) };
    }

    template<typename... Args>
    static auto io_error(std::format_string<Args...> fmt, Args&&... args) -> Status
    {
        return Status{ Code::IOError, std::format(fmt, std::forward<Args>(args)...) };
    }

private:
    enum class Code : uint8_t { 
        OK, 
        NotFound,
        Corruption, 
        NotSupported, 
        InvalidArgument, 
        IOError 
    };

    Code code_ = Code::OK;
    std::string message_;

    Status(Code code, std::string_view message)
      : code_{ code }, 
        message_{ message }
    {}
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_STATUS_H
