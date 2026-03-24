#include <sponge/redis/string.h>

#include <cstring>
#include <memory_resource>
#include <string_view>
#include <utility>

#include <gsl/gsl>

#include "sds.h"

namespace spg::redis {

String::String()
  : String{ std::pmr::get_default_resource() }
{}

String::String(std::string_view str)
  : String{ str, std::pmr::get_default_resource() }
{}

String::String(gsl::not_null<std::pmr::memory_resource*> resource)
  : String{ "", resource }
{}

String::String(std::string_view str, gsl::not_null<std::pmr::memory_resource*> resource)
  : resource_{ resource }
{
    SDS manager{ resource_ };
    sds_ = manager.create(str);
}

String::String(const String& other)
  : resource_{ other.resource_ }
{
    SDS manager{ resource_};
    sds_ = manager.create(other.sds_, manager.capacity(other.sds_));
}

auto String::operator=(const String& other) -> String&
{
    if (this == &other) return *this;

    assign(other.view());
    return *this;
}

String::String(String&& other) noexcept
  : resource_{ other.resource_ }, 
    sds_{ std::exchange(other.sds_, nullptr) }
{}

auto String::operator=(String&& other) -> String&
{
    if (this == &other) return *this;

    if (resource_->is_equal(*other.resource_)) {
        SDS manager{ resource_ };
        manager.destroy(sds_);
        sds_ = std::exchange(other.sds_, nullptr);
    }
    else {
        assign(other.view());
        other.clear();
    }

    return *this;
}

String::~String()
{
    if (sds_ != nullptr) {
        SDS manager{ resource_ };
        manager.destroy(sds_);
        sds_ = nullptr;
    }
}

void String::append(std::string_view str)
{
    auto old_size = size();
    auto new_size = old_size + str.size();

    if (available() >= str.size()) {
        std::memcpy(sds_ + old_size, str.data(), str.size());
    }
    else {
        auto new_capacity = std::max(capacity() * EXPAND_FACTOR, new_size);

        SDS manager{ resource_ };
        auto new_sds = manager.create(sds_, new_capacity);
        manager.destroy(sds_);
        sds_ = new_sds;
    }

    std::memcpy(sds_ + old_size, str.data(), str.size());
    SDS::increase_size(sds_, str.size());
    sds_[new_size] = '\0';
}

void String::reserve(size_t new_capacity)
{
    if (new_capacity <= capacity()) return;

    SDS manager{ resource_ };
    auto new_sds = manager.create(sds_, new_capacity);
    manager.destroy(sds_);
    sds_ = new_sds;
}

void String::reserve(size_t new_capacity, ExpandGreedyTag greedy)
{
    if (new_capacity <= capacity()) return;

    if (new_capacity < MAX_PREALLOC)
        new_capacity *= EXPAND_FACTOR;
    else
        new_capacity += MAX_PREALLOC;

    SDS manager{ resource_ };
    auto new_sds = manager.create(sds_, new_capacity);
    manager.destroy(sds_);
    sds_ = new_sds;
}

void String::resize(size_t new_size)
{
    if (new_size == size()) return;

    if (new_size < size()) {
        SDS::size(sds_, new_size);
        sds_[new_size] = '\0';
        return;
    }
 
    reserve(new_size, greedy);
    std::memset(sds_ + size(), 0, new_size - size());
    SDS::size(sds_, new_size);
}

void String::clear() noexcept
{
    SDS::size(sds_, 0);
    sds_[0] = '\0';
}

void String::shrink_to_fit()
{
    if (available() == 0) return;

    SDS manager{ resource_ };
    auto new_sds = manager.create(sds_, size());
    manager.destroy(sds_);
    sds_ = new_sds;
}

void String::assign(std::string_view str)
{
    if (str.size() > capacity())
        reserve(str.size(), greedy);
    
    std::memcpy(sds_, str.data(), str.size());
    SDS::size(sds_, str.size());
    sds_[str.size()] = '\0';
}

auto String::size() const noexcept -> size_t
{
    return SDS::size(sds_);
}

auto String::capacity() const noexcept -> size_t
{
    return SDS::capacity(sds_);
}

auto String::available() const noexcept -> size_t
{
    return SDS::available(sds_);
}

auto String::view() const noexcept -> std::string_view
{
    return { sds_, size() };
}

auto format_as(const String& str) -> std::string_view
{
    return str.view();
}

} // namespace spg::redis
