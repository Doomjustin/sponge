#ifndef SPONGE_LEVELDB_MEMTABLE_H
#define SPONGE_LEVELDB_MEMTABLE_H

#include <compare>
#include <memory_resource>
#include <string_view>
#include <variant>

#include <gsl/gsl>

#include "formats.h"
#include "skip_list.h"

namespace spg::leveldb {

struct Deleted {};
struct NotFound {};

class MemTable {
public:
    using MemoryResource = std::pmr::memory_resource;
    using ReadResult = std::variant<std::string_view, Deleted, NotFound>;

    explicit MemTable(gsl::not_null<MemoryResource*> mem_resource = std::pmr::get_default_resource());

    void add(SequenceNumber sequence_number, ValueType type, std::string_view key, std::string_view value) noexcept;

    [[nodiscard]]
    auto get(std::string_view key, SequenceNumber sequence_number) const noexcept -> ReadResult;

private:
    struct Query {
        std::string_view key;
        SequenceNumber sequence_number;
    };

    // Varint32(InternalKey_Length) | InternalKey_Bytes | Varint32(Value_Length) | Value_Bytes
    struct PackedKeyComparator {
        [[nodiscard]]
        auto operator()(const std::string_view lhs, const std::string_view rhs) const noexcept -> std::strong_ordering;

        [[nodiscard]]
        auto operator()(const std::string_view key, const Query& query) const noexcept -> std::strong_ordering;

        [[nodiscard]]
        auto operator()(const Query& query, const std::string_view key) const noexcept -> std::strong_ordering;
    };

    using KeySizeType = uint32_t;
    using ValueSizeType = uint32_t;

    MemoryResource* resource_;
    SkipList<std::string_view, PackedKeyComparator> skip_list_;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_MEMTABLE_H
