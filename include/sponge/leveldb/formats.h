#ifndef SPONGE_LEVELDB_FORMATS_H
#define SPONGE_LEVELDB_FORMATS_H

#include <cstdint>

#include <sponge/named_type.h>

namespace spg::leveldb {

enum class ValueType : uint8_t { 
    Deletion = 0, 
    Value = 1 
};

using SequenceNumber = NamedType<std::uint64_t,
                                 "SequenceNumber",
                                 Comparable,
                                 Arithmetic>;

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_FORMATS_H
