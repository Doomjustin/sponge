#include "sds.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <utility>

#include <gsl/gsl>

namespace spg::redis {

SDS::SDS(gsl::not_null<std::pmr::memory_resource*> resource) 
    : resource_{ resource } 
{}

auto SDS::create(std::string_view str) noexcept -> char*
{
    return create(str, str.size());
}

auto SDS::create(std::string_view str, size_t capacity) noexcept -> char*
{
    assert(capacity >= str.size());

    auto required_type = type_for(capacity);

    switch (required_type) {
    case Flag::Type8:
        return create_sds<Type8, Flag::Type8>(str, capacity);
    case Flag::Type16:
        return create_sds<Type16, Flag::Type16>(str, capacity);
    case Flag::Type32:
        return create_sds<Type32, Flag::Type32>(str, capacity);
    case Flag::Type64:
        return create_sds<Type64, Flag::Type64>(str, capacity);
    default:
        std::unreachable();
    }
}

void SDS::destroy(char* sds)
{
    if (sds == nullptr)
        return;

    auto flag = extract_flag(sds);
    switch (flag) {
    case Flag::Type8:
        destroy_sds<Type8>(sds);
        break;
    case Flag::Type16:
        destroy_sds<Type16>(sds);
        break;
    case Flag::Type32:
        destroy_sds<Type32>(sds);
        break;
    case Flag::Type64:
        destroy_sds<Type64>(sds);
        break;
    default:
        std::unreachable();
    }
}

auto SDS::size(gsl::not_null<char*> sds) noexcept -> size_t
{
    auto flag = extract_flag(sds);
    switch (flag) {
    case Flag::Type8: 
        return extract_size<Type8>(sds);
    case Flag::Type16:
        return extract_size<Type16>(sds);
    case Flag::Type32: 
        return extract_size<Type32>(sds);
    case Flag::Type64:
        return extract_size<Type64>(sds);
    default:
        std::unreachable();
    }
}

auto SDS::size(gsl::not_null<char*> sds, size_t new_size) noexcept -> size_t
{
    auto flag = extract_flag(sds);
    switch (flag) {
    case Flag::Type8: 
        return extract_size<Type8>(sds) = new_size;
    case Flag::Type16:
        return extract_size<Type16>(sds) = new_size;
    case Flag::Type32: 
        return extract_size<Type32>(sds) = new_size;
    case Flag::Type64:
        return extract_size<Type64>(sds) = new_size;
    default:
        std::unreachable();      
    }
}

auto SDS::increase_size(gsl::not_null<char*> sds, size_t size) noexcept -> size_t
{
    auto flag = extract_flag(sds);
    switch (flag) {
    case Flag::Type8: 
        return extract_size<Type8>(sds) += size;
    case Flag::Type16:
        return extract_size<Type16>(sds) += size;
    case Flag::Type32: 
        return extract_size<Type32>(sds) += size;
    case Flag::Type64:
        return extract_size<Type64>(sds) += size;
    default:
        std::unreachable();
    } 
}

auto SDS::decrease_size(gsl::not_null<char*> sds, size_t size) noexcept -> size_t
{
    auto raw = sds.get();
    auto flag = static_cast<Flag>(raw[-1]);
    switch (flag) {
    case Flag::Type8: 
        return extract_size<Type8>(raw) -= size;
    case Flag::Type16:
        return extract_size<Type16>(raw) -= size;
    case Flag::Type32: 
        return extract_size<Type32>(raw) -= size;
    case Flag::Type64:
        return extract_size<Type64>(raw) -= size;
    default:
        std::unreachable();
    }
}

auto SDS::capacity(gsl::not_null<char*> sds) noexcept -> size_t
{
    auto raw = sds.get();
    auto flag = static_cast<Flag>(raw[-1]);
    switch (flag) {
    case Flag::Type8:
        return extract_capacity<Type8>(raw);
    case Flag::Type16:
        return extract_capacity<Type16>(raw);
    case Flag::Type32:
        return extract_capacity<Type32>(raw);
    case Flag::Type64:
        return extract_capacity<Type64>(raw);
    default:
        std::unreachable();
    }
}

auto SDS::capacity(gsl::not_null<char*> sds, size_t new_capacity) noexcept -> size_t
{
    auto flag = extract_flag(sds);
    switch (flag) {
    case Flag::Type8:
        return extract_capacity<Type8>(sds) = new_capacity;
    case Flag::Type16:
        return extract_capacity<Type16>(sds) = new_capacity;
    case Flag::Type32:
        return extract_capacity<Type32>(sds) = new_capacity;
    case Flag::Type64:
        return extract_capacity<Type64>(sds) = new_capacity;
    default:
        std::unreachable();
    }
}

auto SDS::available(gsl::not_null<char*> sds) noexcept -> size_t
{
    auto flag = extract_flag(sds);
    switch (flag) {
    case Flag::Type8:
        return extract_available<Type8>(sds);
    case Flag::Type16:
        return extract_available<Type16>(sds);
    case Flag::Type32:
        return extract_available<Type32>(sds);
    case Flag::Type64:
        return extract_available<Type64>(sds);
    default:
        std::unreachable();
    }
}

auto SDS::type_for(size_t length) -> Flag
{
    if (length <= std::numeric_limits<Type8>::max())
        return Flag::Type8;
    
    if (length <= std::numeric_limits<Type16>::max())
        return Flag::Type16;

    if (length <= std::numeric_limits<Type32>::max())
        return Flag::Type32;

    return Flag::Type64;
}

} // namespace spg::redis
