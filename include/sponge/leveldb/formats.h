#ifndef SPONGE_LEVELDB_FORMATS_H
#define SPONGE_LEVELDB_FORMATS_H

#include <cstdint>

namespace spg::leveldb {

enum class ValueType : uint8_t { 
    Deletion = 0, 
    Value = 1 
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_FORMATS_H
