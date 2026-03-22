#include <sponge/hash.h>

#include <cstddef>
#include <functional>
#include <string_view>

namespace spg {

auto hash(std::string_view key, UseStdTag tag) -> size_t
{
    using Hasher = std::hash<std::string_view>;
    return Hasher{}(key);
}

[[nodiscard]]
auto hash(std::string_view key, UseFNV1aTag tag) -> size_t
{
    constexpr size_t FNV_offset_basis = 14695981039346656037ULL;
    constexpr size_t FNV_prime = 1099511628211ULL;

    size_t result = FNV_offset_basis;
    for (unsigned char byte : key) {
        result ^= byte;
        result *= FNV_prime;
    }
    
    return result;
}

} // namespace spg
