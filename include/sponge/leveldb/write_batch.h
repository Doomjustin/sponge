#ifndef SPONGE_LEVELDB_WRITE_BATCH_H
#define SPONGE_LEVELDB_WRITE_BATCH_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>

#include <gsl/pointers>

#include <sponge/coding.h>

#include "exceptions.h"
#include "formats.h"

namespace spg::leveldb {

template<typename Handler>
concept WriteBatchHandler = requires(Handler handler, std::string_view key, std::string_view value) 
{
    { handler.put(key, value) } -> std::same_as<void>;
    { handler.erase(key) } -> std::same_as<void>;
};


/*
 * | <--- 8 字节 --->  | <--- 4 字节 ---> |  <----------- 变长记录序列 (Records) -----------> |
 * +------------------+------------------+------------------------------------------------+
 * | SequenceNumber   |  记录总数 (Count) | Record 1 | Record 2 | Record 3 | ...            |
 * +------------------+------------------+------------------------------------------------+
 */
class WriteBatch {
public:
    using MemoryResource = std::pmr::memory_resource;

    explicit WriteBatch(gsl::not_null<MemoryResource*> resource = std::pmr::get_default_resource());

    void clear();

    void put(std::string_view key, std::string_view value);

    void erase(std::string_view key);

    [[nodiscard]]
    auto sequence() const noexcept -> SequenceNumber;

    void set_sequence(SequenceNumber new_number);

    [[nodiscard]]
    constexpr auto approximate_size() const -> size_t
    {
        return rep_.size();
    }

    [[nodiscard]]
    auto encode() const noexcept -> std::string_view
    {
        return rep_;
    }

    template<WriteBatchHandler Handler>
    void iterate(Handler&& handler) const
    {
        assert(rep_.size() >= HEADER_SIZE);   

        std::string_view records{ rep_.data() + HEADER_SIZE, rep_.size() - HEADER_SIZE };

        for (uint32_t i = 0; i < count(); ++i) {
            if (records.empty())
                throw CorruptionException{ "invalid WriteBatch: not enough records" };

            auto [type, _] = fixed::decode<uint8_t>(records.begin());
            auto value_type = static_cast<ValueType>(type);
            records.remove_prefix(1);

            auto [key_size, iter] = varint::decode<SizeType>(records.begin());
            records.remove_prefix(iter - records.begin());
            
            std::string_view key{ records.data(), key_size };
            records.remove_prefix(key_size);
            
            switch (value_type) {
            case ValueType::Value:
            {
                auto [value_size, iter] = varint::decode<SizeType>(records.begin());
                records.remove_prefix(iter - records.begin());

                std::string_view value{ records.data(), value_size };
                records.remove_prefix(value_size);

                handler.put(key, value);
                break;
            }
            case ValueType::Deletion:
                handler.erase(key);
                break;
            default:
                std::unreachable();
            }
        }
    }

private:
    using SizeType = uint32_t;
    using SequenceNumberType = SequenceNumber::value_type;

    static constexpr size_t SEQUENCE_NUMBER_SIZE = sizeof(SequenceNumberType);
    static constexpr size_t COUNT_SIZE = sizeof(SizeType);
    static constexpr size_t HEADER_SIZE = SEQUENCE_NUMBER_SIZE + COUNT_SIZE;

    std::pmr::string rep_;

    [[nodiscard]]
    auto count() const noexcept -> uint32_t;

    void set_count(uint32_t new_count);

    [[nodiscard]]
    constexpr auto count_begin() const noexcept -> const char* 
    { 
        return rep_.data() + SEQUENCE_NUMBER_SIZE; 
    }

    [[nodiscard]]
    constexpr auto count_begin() noexcept -> char* 
    { 
        return rep_.data() + SEQUENCE_NUMBER_SIZE; 
    }
};
    
} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_WRITE_BATCH_H
