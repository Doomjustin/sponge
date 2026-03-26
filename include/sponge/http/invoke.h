#ifndef SPONGE_HTTP_INVOKE_H
#define SPONGE_HTTP_INVOKE_H

#include <charconv>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <glaze/glaze.hpp>

#include "exceptions.h"
#include "request.h"

namespace spg::http {

template<typename T>
struct FunctionTraits: public FunctionTraits<decltype(std::function{ std::declval<std::decay_t<T>>() })> {};

template<typename ReturnT, typename... Args>
struct FunctionTraits<std::function<ReturnT(Args...)>> {
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<typename ReturnT, typename... Args>
constexpr auto overload_cast(ReturnT (*func)(Args...)) -> ReturnT (*)(Args...)
{
    return func;
}

// URL capture group types: string / string_view / arithmetic
template<typename T>
constexpr bool is_url_param_v =
    std::is_same_v<std::remove_cvref_t<T>, std::string> ||
    std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
    std::is_integral_v<std::remove_cvref_t<T>> || std::is_floating_point_v<std::remove_cvref_t<T>>;

template<typename T>
auto parse_argument(std::string_view arg) -> T
{
    if constexpr (std::is_same_v<T, std::string>) {
        return std::string{ arg };
    }
    else if constexpr (std::is_same_v<T, std::string_view>) {
        return arg;
    }
    else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        T value{};
        std::from_chars(arg.data(), arg.data() + arg.size(), value);
        return value;
    }
    else
        static_assert(!sizeof(T*), "Unsupported argument type");
}

template<typename T>
auto parse_json_body(std::string_view body) -> T
{
    T value{};
    if (auto ec = glz::read_json(value, std::string{ body }); ec)
        throw BadRequestException{ "Invalid JSON body" };

    return value;
}

// Count URL param types in ArgsTuple at positions [1, N)
template<typename ArgsTuple, size_t N>
constexpr auto count_url_params_before() -> size_t
{
    if constexpr (N <= 1)
        return 0;
    else
        return count_url_params_before<ArgsTuple, N - 1>() 
            + (is_url_param_v<std::tuple_element_t<N - 1, ArgsTuple>> ? 1 : 0);
}

// Inject a single argument: from URL capture group or from JSON body
template<typename ArgsTuple, size_t I, typename MatchResult>
auto get_arg(const Request& request, const MatchResult& match)
    -> std::remove_cvref_t<std::tuple_element_t<I + 1, ArgsTuple>>
{
    using T = std::tuple_element_t<I + 1, ArgsTuple>;

    if constexpr (is_url_param_v<T>) {
        constexpr size_t capture_idx = count_url_params_before<ArgsTuple, I + 1>() + 1;
        return parse_argument<std::remove_cvref_t<T>>(match.template get<capture_idx>().to_view());
    }
    else {
        return parse_json_body<std::remove_cvref_t<T>>(request.body());
    }
}

// Count JSON body params (non-URL params) in ArgsTuple at positions [1, Arity)
template<typename ArgsTuple, size_t Arity>
constexpr auto count_json_params() -> size_t
{
    if constexpr (Arity <= 1)
        return 0;
    else
        return count_json_params<ArgsTuple, Arity - 1>() 
            + (is_url_param_v<std::tuple_element_t<Arity - 1, ArgsTuple>> ? 0 : 1);
}

template<typename Traits, typename Func, typename MatchResult, size_t... Is>
auto invoke_handler_impl(Func&& func, 
                         const Request& request, 
                         const MatchResult& match, 
                         std::index_sequence<Is...>)
{
    using ArgsTuple = typename Traits::args_tuple;

    static_assert(count_json_params<ArgsTuple, Traits::arity>() <= 1, 
                  "Handler may have at most one JSON body parameter");

    return func(request, get_arg<ArgsTuple, Is>(request, match)...);
}

template<typename Func, typename MatchResult>
    requires std::is_invocable_v<std::decay_t<Func>, const Request&>
auto invoke_handler(Func&& func, const Request& request, const MatchResult& match)
{
    return func(request);
}

template<typename Func, typename MatchResult>
    requires(!std::is_invocable_v<std::decay_t<Func>, const Request&>)
auto invoke_handler(Func&& func, const Request& request, const MatchResult& match)
{
    using StdFunc = decltype(std::function{ std::declval<std::decay_t<Func>>() });
    using Traits = FunctionTraits<StdFunc>;
    
    return invoke_handler_impl<Traits>(std::forward<Func>(func), 
                                       request, 
                                       match,
                                       std::make_index_sequence<Traits::arity - 1>{});
}

} // namespace spg::http


#endif // SPONGE_HTTP_INVOKE_H
