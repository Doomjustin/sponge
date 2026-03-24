#ifndef SPONGE_PATH_H
#define SPONGE_PATH_H

#include <filesystem>
#include <string_view>

namespace spg {

// 如果path是相对路径，那么会拼接成当前工作目录的绝对路径
auto full_path(std::string_view path) -> std::filesystem::path;

} // namespace spg

#endif // SPONGE_PATH_H
