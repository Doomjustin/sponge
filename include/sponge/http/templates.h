#ifndef SPONGE_HTTP_TEMPLATES_H
#define SPONGE_HTTP_TEMPLATES_H

#include "request.h"
#include "status.h"

namespace spg::http {

struct templates {
    templates() = delete;

    static auto method_not_allowed(const Request& request) -> std::string;

    static auto not_found(const Request& request) -> std::string;

    static auto error(const Request& request, Status status) -> std::string;

    static constexpr const char* CONTENT_TEXT = "text/plain";

    static constexpr const char* CONTENT_HTML = "text/html";

    static constexpr const char* CONTENT_JSON = "application/json";
};

} // namespace spg::http

#endif // SPONGE_HTTP_TEMPLATES_H
