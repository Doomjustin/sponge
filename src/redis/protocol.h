#ifndef SPONGE_REDIS_PROTOCOL_H
#define SPONGE_REDIS_PROTOCOL_H

#include <string_view>
#include <vector>

namespace spg::redis {

struct resp {
    struct Command {
        using Arguments = std::pmr::vector<std::string_view>;
        Arguments arguments;
        std::string_view raw;
    };
    
    using Commands = std::pmr::vector<Command>;

    struct ParseResult {
        Commands commands;
        std::size_t consumed_bytes = 0;

        ParseResult(std::pmr::memory_resource* resource)
          : commands{ resource }
        {}
    };

    static auto parse_request(std::string_view buffer, std::pmr::memory_resource* resource) -> ParseResult;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_PROTOCOL_H
