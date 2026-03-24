#ifndef SPONGE_REDIS_SDS_H
#define SPONGE_REDIS_SDS_H

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory_resource>
#include <string_view>

#include <gsl/gsl>

namespace spg::redis {

class SDS {
public:
    explicit SDS(gsl::not_null<std::pmr::memory_resource*> resource);

    auto create(std::string_view str) noexcept -> char*;
    
    auto create(std::string_view str, size_t capacity) noexcept -> char*;

    void destroy(char* sds);

    static auto size(gsl::not_null<char*> sds) noexcept -> size_t;

    static auto size(gsl::not_null<char*> sds, size_t new_size) noexcept -> size_t;

    static auto increase_size(gsl::not_null<char*> sds, size_t size) noexcept -> size_t;

    static auto decrease_size(gsl::not_null<char*> sds, size_t size) noexcept -> size_t;

    static auto capacity(gsl::not_null<char*> sds) noexcept -> size_t;

    static auto capacity(gsl::not_null<char*> sds, size_t new_capacity) noexcept -> size_t;

    static auto available(gsl::not_null<char*> sds) noexcept -> size_t;

private:
    using Allocator = std::pmr::polymorphic_allocator<std::byte>;
    using Type8 = uint8_t;
    using Type16 = uint16_t;
    using Type32 = uint32_t;
    using Type64 = uint64_t;
    std::pmr::memory_resource* resource_;
    
    enum class Flag : uint8_t {
        Type8,
        Type16,
        Type32,
        Type64
    };

    template<std::unsigned_integral Size>
    struct __attribute__((packed)) BasicSDS {
        Size size;
        Size capacity;
        Flag flag;

        auto data() noexcept -> char* 
        {
            return reinterpret_cast<char*>(this + 1);
        }

        [[nodiscard]]
        auto data() const noexcept -> const char* 
        {
            return reinterpret_cast<const char*>(this + 1);
        }

        auto operator[](size_t index) noexcept -> char&
        {
            return data()[index];
        }

        [[nodiscard]]
        auto operator[](size_t index) const noexcept -> const char&
        {
            return data()[index];
        }

        [[nodiscard]]
        constexpr auto avail() const noexcept -> Size
        {
            return capacity - size;
        }
    };

    static auto type_for(size_t length) -> Flag;
            
    static auto extract_flag(char* sds) noexcept -> Flag
    {
        return static_cast<Flag>(static_cast<uint8_t>(sds[-1]));
    }

    template<std::unsigned_integral Size>
    static auto extract_capacity(char* sds) noexcept -> Size&
    {
        using SDS = BasicSDS<Size>;
        auto header = reinterpret_cast<SDS*>(sds - sizeof(SDS));
        return header->capacity;
    } 

    template<std::unsigned_integral Size>
    static auto extract_size(char* sds) noexcept -> Size&
    {
        using SDS = BasicSDS<Size>;
        auto header = reinterpret_cast<SDS*>(sds - sizeof(SDS));
        return header->size;
    }
    
    template<std::unsigned_integral Size>
    static auto extract_available(char* sds) noexcept -> size_t
    {
        using SDS = BasicSDS<Size>;
        auto header = reinterpret_cast<SDS*>(sds - sizeof(SDS));
        return header->avail();
    }

    template<std::unsigned_integral Size, Flag flag>
    auto create_sds(std::string_view str, size_t capacity) noexcept -> char*
    {
        using SDS = BasicSDS<Size>;
        assert(capacity >= str.size());

        capacity = std::max(capacity, str.size());
        auto total_size = sizeof(SDS) + capacity + 1; // +1 for null terminator
        // 必须按Size类型对齐分配，以确保SDS头部正确对齐
        auto raw = resource_->allocate(total_size, alignof(Size));

        auto* sds = std::construct_at(reinterpret_cast<SDS*>(raw), 
                                      str.size(), 
                                      capacity, 
                                      flag);
                                      
        std::memcpy(sds->data(), str.data(), str.size());
        (*sds)[str.size()] = '\0'; // Ensure null termination

        return sds->data();     
    }

    template<std::unsigned_integral Size>
    void destroy_sds(char* sds) noexcept
    {
        using SDS = BasicSDS<Size>;
        auto* header = reinterpret_cast<SDS*>(sds - sizeof(SDS));
        auto total_size = sizeof(SDS) + header->capacity + 1; // +1 for null terminator

        std::destroy_at(header);
        resource_->deallocate(header, total_size, alignof(Size));
    }
};

} // namespace spg::redis

#endif // SPONGE_REDIS_SDS_H
