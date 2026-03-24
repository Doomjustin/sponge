#ifndef SPONGE_LEVELDB_WRITE_BATCH_H
#define SPONGE_LEVELDB_WRITE_BATCH_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "formats.h"
#include "status.h"

namespace spg::leveldb {

template<typename T>
concept WriteBatchHandler = requires(T handler, std::string_view key, std::string_view value) {
    { handler.put(key, value) } -> std::same_as<void>;
    { handler.erase(key) } -> std::same_as<void>;
};

class WriteBatch {
public:
    using SequenceNumber = uint64_t;

    WriteBatch() { rep_.resize(HEADER_SIZE); }

    void clear();

    void put(std::string_view key, std::string_view value);

    void erase(std::string_view key);

    template<WriteBatchHandler Handler>
    auto for_each(Handler&& handler) const -> Status
    {
        std::span input{ rep_ };
        if (input.size() < HEADER_SIZE)
            return Status::corruption("malformed WriteBatch");

        // skip header
        input = input.subspan(HEADER_SIZE);
        uint32_t found_records = 0;
        uint32_t expected_records = count();

        while (!input.empty()) {
            ++found_records;

            auto type_raw = std::to_integer<uint8_t>(input[0]);
            auto type = static_cast<ValueType>(type_raw);
            input = input.subspan(1);

            if (type == ValueType::Value) {
                auto [key, consumed] = extract(input);
                if (consumed == 0)
                    return Status::corruption("malformed WriteBatch: invalid key");

                auto [value, value_consumed] = extract(input.subspan(consumed));
                if (value_consumed == 0)
                    return Status::corruption("malformed WriteBatch: invalid value");

                handler.put(key, value);
                input = input.subspan(consumed + value_consumed);
            }
            else if (type == ValueType::Deletion) {
                auto [key, consumed] = extract(input);
                if (consumed == 0)
                    return Status::corruption("malformed WriteBatch: invalid key");

                handler.erase(key);
                input = input.subspan(consumed);
            }
            else {
                return Status::corruption("unknown ValueType in WriteBatch");
            }
        }

        if (found_records != expected_records)
            return Status::corruption("malformed WriteBatch: record count mismatch");

        return Status::ok();
    }

    [[nodiscard]]
    auto data() const -> std::span<const std::byte>
    {
        return rep_;
    }

    [[nodiscard]]
    constexpr auto approximate_size() const -> size_t
    {
        return rep_.size();
    }

    [[nodiscard]]
    auto count() const noexcept -> uint32_t;

    [[nodiscard]]
    auto sequence() const noexcept -> SequenceNumber;

    void sequence(SequenceNumber seq);

private:
    static constexpr size_t HEADER_SIZE = 12;

    // header layout:
    // 0-7: sequence number
    // 8-11: count
    // rest: data
    // data layout:
    //   value type (1 byte)
    //   key size (varint)
    //   key bytes
    //   value size (varint, only for value type)
    //   value bytes (only for value type)
    std::vector<std::byte> rep_;

    void increment_count();

    auto count_view() noexcept -> std::span<std::byte> { return std::span{ rep_.data() + 8, 4 }; }

    [[nodiscard]]
    auto count_view() const noexcept -> std::span<const std::byte>
    {
        return std::span{ rep_.data() + 8, 4 };
    }

    static auto extract(std::span<const std::byte> src) -> std::pair<std::string_view, std::size_t>;
};
    
} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_WRITE_BATCH_H
