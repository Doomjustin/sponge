#ifndef SPONGE_HTTP_RESPONSE_H
#define SPONGE_HTTP_RESPONSE_H

#include <string>
#include <utility>
#include <vector>

#include "status.h"

namespace spg::http {

class Response {
public:
    using Header = std::pair<std::string, std::string>;

    auto status(Status value) -> Response&
    {
        status_ = value;
        return *this;
    }

    auto body(std::string value) -> Response&
    {
        body_ = std::move(value);
        return *this;
    }

    auto content_type(std::string value) -> Response&
    {
        content_type_ = std::move(value);
        return *this;
    }

    // set_header replaces existing entries with the same key.
    auto set_header(std::string key, std::string value) -> Response&
    {
        std::erase_if(headers_, [&key](const auto& item) { return item.first == key; });
        headers_.emplace_back(std::move(key), std::move(value));
        return *this;
    }

    // add_header keeps existing entries, useful for duplicate headers like Set-Cookie.
    auto add_header(std::string key, std::string value) -> Response&
    {
        headers_.emplace_back(std::move(key), std::move(value));
        return *this;
    }

    auto set_cookie(std::string value) -> Response&
    {
        return add_header("set-cookie", std::move(value));
    }

    [[nodiscard]] 
    auto status() const noexcept -> Status { return status_; }

    [[nodiscard]] 
    auto body() const noexcept -> const std::string& { return body_; }

    [[nodiscard]] 
    auto content_type() const noexcept -> const std::string& { return content_type_; }

    [[nodiscard]] 
    auto headers() const noexcept -> const std::vector<Header>& { return headers_; }

private:
    Status status_{ Status::Ok };
    std::string body_{};
    std::string content_type_{ "text/plain" };
    std::vector<Header> headers_{};
};

} // namespace spg::http

#endif // SPONGE_HTTP_RESPONSE_H
