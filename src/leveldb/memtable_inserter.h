#ifndef SPONGE_LEVELDB_MEMTABLE_INSERTER_H
#define SPONGE_LEVELDB_MEMTABLE_INSERTER_H

#include <string_view>

#include <sponge/leveldb/memtable.h>
#include <sponge/leveldb/write_batch.h>

namespace spg::leveldb {

struct MemTableInserter {
    SequenceNumber sequence_number;
    MemTable& memtable;

    MemTableInserter(SequenceNumber seq, MemTable& memtable)
      : sequence_number(seq), memtable(memtable)
    {}

    void put(const std::string_view key, const std::string_view value)
    {
        memtable.add(sequence_number, ValueType::Value, key, value);
        ++sequence_number;
    }

    void erase(const std::string_view key)
    {
        memtable.add(sequence_number, ValueType::Deletion, key, {});
        ++sequence_number;
    }
};


void apply_to(const WriteBatch& batch, MemTable& table)
{
    auto start = batch.sequence();
    MemTableInserter inserter{ start, table };
    batch.iterate(inserter);
}

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_MEMTABLE_INSERTER_H
