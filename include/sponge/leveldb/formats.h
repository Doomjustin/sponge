#ifndef SPONGE_LEVELDB_FORMATS_H
#define SPONGE_LEVELDB_FORMATS_H

#include <cstdint>

#include <sponge/named_type.h>

namespace spg::leveldb {

enum class ValueType : uint8_t { 
    Deletion = 0, 
    Value = 1 
};

enum class RecordType: uint8_t {
    Zero,
    Full,
    First,
    Middle,
    Last
};

using SequenceNumber = NamedType<std::uint64_t,
                                 "SequenceNumber",
                                 Comparable,
                                 Arithmetic>;

inline constexpr size_t LOG_BLOCK_SIZE = 32 * 1024; // 32KB

} // namespace spg::leveldb

namespace spg::leveldb::literals {

[[nodiscard]]
constexpr auto operator""_seq(unsigned long long value) noexcept -> SequenceNumber
{
    // 显式强转，消除从 unsigned long long 到 uint64_t 的潜在警告
    return SequenceNumber{ static_cast<std::uint64_t>(value) };
}

} // namespace spg::leveldb::literals

#endif // SPONGE_LEVELDB_FORMATS_H
