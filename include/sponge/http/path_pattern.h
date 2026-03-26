#ifndef SPONGE_HTTP_PATH_PATTERN_H
#define SPONGE_HTTP_PATH_PATTERN_H

#include <array>
#include <cstddef>
#include <cstdint>

#include <ctre.hpp>

namespace spg::http {

enum class PathConverter : std::uint8_t {
    Str,
    Int,
    Float,
    Path,
    Slug,
};

constexpr auto is_regex_meta(char ch) -> bool
{
    switch (ch) {
    case '\\':
    case '.':
    case '^':
    case '$':
    case '|':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '*':
    case '+':
    case '?':
        return true;
    default:
        return false;
    }
}

template<ctll::fixed_string pattern>
consteval auto parameter_end(std::size_t start) -> std::size_t
{
    auto end = start;
    while (end < pattern.size() && pattern[end] != '>')
        ++end;

    if (end == pattern.size())
        throw "Unterminated path parameter";

    return end;
}

template<ctll::fixed_string pattern>
consteval auto converter_name_end(std::size_t start, std::size_t end) -> std::size_t
{
    auto name_end = start;
    while (name_end < end && pattern[name_end] != ':')
        ++name_end;
    return name_end;
}

template<ctll::fixed_string pattern>
consteval auto token_equals(std::size_t start, std::size_t end, const char* token) -> bool
{
    auto index = std::size_t{};
    for (auto pos = start; pos < end; ++pos, ++index) {
        if (token[index] == '\0' || static_cast<char>(pattern[pos]) != token[index])
            return false;
    }
    return token[index] == '\0';
}

template<ctll::fixed_string pattern>
consteval auto parse_converter(std::size_t start, std::size_t end) -> PathConverter
{
    const auto converter_end = converter_name_end<pattern>(start, end);

    if (token_equals<pattern>(start, converter_end, "str"))
        return PathConverter::Str;
    if (token_equals<pattern>(start, converter_end, "int"))
        return PathConverter::Int;
    if (token_equals<pattern>(start, converter_end, "float"))
        return PathConverter::Float;
    if (token_equals<pattern>(start, converter_end, "path"))
        return PathConverter::Path;
    if (token_equals<pattern>(start, converter_end, "slug"))
        return PathConverter::Slug;

    throw "Unsupported path converter";
}

constexpr auto converter_regex_size(PathConverter converter) -> std::size_t
{
    switch (converter) {
    case PathConverter::Str:
        return 7; // ([^/]+)
    case PathConverter::Int:
        return 10; // (-?[0-9]+)
    case PathConverter::Float:
        return 23; // (-?[0-9]+(?:\.[0-9]+)?)
    case PathConverter::Path:
        return 4; // (.+)
    case PathConverter::Slug:
        return 16; // ([A-Za-z0-9_-]+)
    }

    return 0;
}

template<std::size_t N, std::size_t M>
consteval void append_fragment(std::array<char, N>& output, std::size_t& cursor,
                               const char (&fragment)[M])
{
    for (std::size_t i = 0; i < M - 1; ++i)
        output[cursor++] = fragment[i];
}

template<std::size_t N>
consteval void append_converter(std::array<char, N>& output, std::size_t& cursor,
                                PathConverter converter)
{
    switch (converter) {
    case PathConverter::Str:
        append_fragment(output, cursor, "([^/]+)");
        return;
    case PathConverter::Int:
        append_fragment(output, cursor, "(-?[0-9]+)");
        return;
    case PathConverter::Float:
        append_fragment(output, cursor, "(-?[0-9]+(?:\\.[0-9]+)?)");
        return;
    case PathConverter::Path:
        append_fragment(output, cursor, "(.+)");
        return;
    case PathConverter::Slug:
        append_fragment(output, cursor, "([A-Za-z0-9_-]+)");
        return;
    }
}

template<ctll::fixed_string pattern>
consteval auto path_pattern_regex_size() -> std::size_t
{
    auto size = std::size_t{};

    for (auto index = std::size_t{}; index < pattern.size(); ++index) {
        if (pattern[index] == '<') {
            const auto end = parameter_end<pattern>(index + 1);
            size += converter_regex_size(parse_converter<pattern>(index + 1, end));
            index = end;
            continue;
        }

        size += is_regex_meta(static_cast<char>(pattern[index])) ? 2 : 1;
    }

    return size;
}

template<ctll::fixed_string pattern>
consteval auto path_pattern_to_regex()
{
    std::array<char, path_pattern_regex_size<pattern>()> output{};
    auto cursor = std::size_t{};

    for (auto index = std::size_t{}; index < pattern.size(); ++index) {
        if (pattern[index] == '<') {
            const auto end = parameter_end<pattern>(index + 1);
            append_converter(output, cursor, parse_converter<pattern>(index + 1, end));
            index = end;
            continue;
        }

        const auto ch = static_cast<char>(pattern[index]);
        if (is_regex_meta(ch))
            output[cursor++] = '\\';
        output[cursor++] = ch;
    }

    return ctll::fixed_string{ output };
}

} // namespace spg::http

#endif // SPONGE_HTTP_PATH_PATTERN_H
