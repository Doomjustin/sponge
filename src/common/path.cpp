#include <sponge/path.h>

namespace spg {

auto full_path(std::string_view path) -> std::filesystem::path
{
    std::filesystem::path p{ path };
    if (p.is_absolute())
        return p;

    auto cwd = std::filesystem::current_path();
    return cwd / p;
}

} // namespace spg
