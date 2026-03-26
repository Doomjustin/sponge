#ifndef SPONGE_HTTP_EXCEPTIONS_H
#define SPONGE_HTTP_EXCEPTIONS_H

#include <exception>
#include <string>

namespace spg::http {

class BadRequestException : public std::exception {
public:
    explicit BadRequestException(std::string message)
      : message_{ std::move(message) }
    {}

    [[nodiscard]] 
    auto what() const noexcept -> const char* override 
    { 
        return message_.c_str(); 
    }

private:
    std::string message_;
};

} // namespace spg::http

#endif // SPONGE_HTTP_EXCEPTIONS_H
