#include "commands.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <sponge/utility.h>

#include "command.h"

namespace spg::redis {
namespace {

template<typename T>
struct FunctionTraits;

template<typename Return, typename Context, typename... Args>
struct FunctionTraits<Return(*)(Context&, Args...)> {
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

using CommandHandler = void(*)(CommandContext&, std::span<const std::string_view>);

template<typename T>
auto parse_arg(std::string_view sv) -> T
{
    if constexpr (std::is_same_v<T, std::string_view>) {
        return sv;
    }
    else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        auto value = numeric_cast<T>(sv);
        if (!value)
            throw std::invalid_argument("Failed to parse argument: " + std::string(sv));
        return *value;
    }
    else {
        static_assert(false, "Unsupported argument type");
    }
}

// 将命令参数确定的函数绑定成 CommandHandler，方便后续调用
template <auto Func>
constexpr auto bind_command() -> CommandHandler
{
    using Traits = FunctionTraits<decltype(Func)>;
    constexpr size_t expected_arity = Traits::arity;

    return +[](CommandContext& context, std::span<const std::string_view> args) -> void
    {
        if (args.size() != expected_arity) {
            context.reply.append(Error{ "ERR wrong number of arguments" });
            return;
        }

        auto invoker = [&]<size_t... Is>(std::index_sequence<Is...>) -> void
        {
            Func(context, parse_arg<std::tuple_element_t<Is, typename Traits::args_tuple>>(args[Is])...);
        };

        try {
            invoker(std::make_index_sequence<expected_arity>{});
        }
        catch (const std::exception& ex) {
            context.reply.append(Error{ fmt::format("ERR {}", ex.what()) });
        }
    };
}

enum class Type: uint8_t {
    Read,
    Write
};

struct Command {
    std::string_view name;
    CommandHandler handler;
    Type type;
};

constexpr std::array<Command, 15> commands {
    // -------- key commands --------
    Command{ .name="EXISTS", .handler=&command::exists, .type=Type::Read },
    Command{ .name="DEL", .handler=&command::del, .type=Type::Write },
    Command{ .name="EXPIRE", .handler=bind_command<&command::expire>(), .type=Type::Write },
    Command{ .name="TTL", .handler=bind_command<&command::ttl>(), .type=Type::Read },
    Command{ .name="PERSIST", .handler=bind_command<&command::persist>(), .type=Type::Write },
    Command{ .name="TYPE", .handler=bind_command<&command::type>(), .type=Type::Read },
    Command{ .name="RENAME", .handler=bind_command<&command::rename>(), .type=Type::Write },
    // -------- string commands --------
    Command{ .name="SET", .handler=&command::set, .type=Type::Write },
    Command{ .name="GET", .handler=bind_command<&command::get>(), .type=Type::Read },
    Command{ .name="STRLEN", .handler=bind_command<&command::strlen>(), .type=Type::Read },
    // ------ sorted set commands ------
    Command{ .name="ZADD", .handler=&command::zadd, .type=Type::Write },
    Command{ .name="ZSCORE", .handler=bind_command<&command::zscore>(), .type=Type::Read },
    // ------- other commands --------
    Command{ .name="BGREWRITEAOF", .handler=bind_command<&command::bg_rewrite_aof>(), .type=Type::Read },
    Command{ .name="FLUSHALL", .handler=&command::flushall, .type=Type::Write },
    Command{ .name="DBSIZE", .handler=bind_command<&command::dbsize>(), .type=Type::Read },
};

// 为了后续的二分查找，我们需要保证 commands 数组是按 name 排序的
consteval auto build_sorted_commands()
{
    auto cmds = commands;
    std::ranges::sort(cmds,[] (const auto& lhs, const auto& rhs) { return lhs.name < rhs.name;});
    return cmds;
}

constexpr auto sorted_commands = build_sorted_commands();

} // namespace


void commands::dispatch(CommandContext& context, const resp::Command& cmd)
{
    auto& arguments = cmd.arguments;

    if (arguments.empty()) {
        context.reply.append(Error{ "ERR empty command" });
        return;
    }

    auto name = to_uppercase(arguments[0]);
    auto it = std::ranges::lower_bound(sorted_commands, 
                                                        std::string_view{ name }, 
                                                        std::ranges::less{}, 
                                                        &Command::name);

    if (it != sorted_commands.end() && it->name == name) {
        if (it->type == Type::Write && !context.aof().is_healthy()) {
            context.reply.append(Error{"MISCONF AOF is configured to save data, but is currently not able to persist on disk. Commands that may modify the data set are disabled."});
            return;
        }
        
        // if (it->type == Type::Write && !context.is_aof_loading)
        //     context.append(cmd.raw);
        
        std::span<const std::string_view> args{ arguments.data() + 1, arguments.size() - 1 };
        it->handler(context, args);
    }
    else {
        context.reply.append(Error{ fmt::format("ERR unknown command '{}'", name) });
    }
}

} // namespace spg::redis
