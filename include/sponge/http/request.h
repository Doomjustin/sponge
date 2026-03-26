#ifndef SPONGE_HTTP_REQUEST_H
#define SPONGE_HTTP_REQUEST_H

#include <optional>
#include <string_view>
#include <unordered_map>

#include "method.h"

namespace spg::http {

class Request {
    friend class RequestBuilder;

public:
    [[nodiscard]]
    auto get_header(std::string_view name) const -> std::optional<std::string_view>
    {
        auto it = headers_.find(name);
        if (it != headers_.end())
            return it->second;

        return {};
    }

    [[nodiscard]]
    constexpr auto method() const noexcept -> Method
    {
        return method_;
    }

    [[nodiscard]]
    constexpr auto path() const noexcept -> std::string_view
    {
        return path_;
    }

    [[nodiscard]]
    constexpr auto body() const noexcept -> std::string_view
    {
        return body_;
    }

    [[nodiscard]]
    constexpr auto version() const noexcept -> unsigned
    {
        return version_;
    }

    [[nodiscard]]
    constexpr auto keep_alive() const noexcept -> bool
    {
        return keep_alive_;
    }

private:
    Method method_ = Method::None;
    std::string_view path_{};
    std::string_view body_{};
    unsigned version_ = 0;
    bool keep_alive_ = false;
    std::unordered_map<std::string_view, std::string_view> headers_;
};


class RequestBuilder {
public:
    auto method(Method method) -> RequestBuilder&
    {
        request_.method_ = method;
        return *this;
    }

    auto path(std::string_view path) -> RequestBuilder&
    {
        request_.path_ = path;
        return *this;
    }

    auto body(std::string_view body) -> RequestBuilder&
    {
        request_.body_ = body;
        return *this;
    }

    auto version(unsigned version) -> RequestBuilder&
    {
        request_.version_ = version;
        return *this;
    }

    auto keep_alive(bool keep_alive) -> RequestBuilder&
    {
        request_.keep_alive_ = keep_alive;
        return *this;
    }

    auto add_header(std::string_view name, std::string_view value) -> RequestBuilder&
    {
        request_.headers_.emplace(name, value);
        return *this;
    }

    auto headers(std::unordered_map<std::string_view, std::string_view> headers) -> RequestBuilder&
    {
        request_.headers_ = std::move(headers);
        return *this;
    }

    auto build() -> Request { return std::move(request_); }

private:
    Request request_;
};

} // namespace spg::http

#endif // SPONGE_HTTP_REQUEST_H
