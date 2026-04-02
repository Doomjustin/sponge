#ifndef SPONGE_LEVELDB_DATABASE_H
#define SPONGE_LEVELDB_DATABASE_H

#include <memory>
#include <optional>

#include "options.h"

namespace spg::leveldb {

class WriteBatch;

class Snapshot;

struct DB {
    virtual ~DB() = default;

    static auto open(const Options& options, std::string_view db_path) -> std::unique_ptr<DB>;

    virtual void put(const WriteOptions& options, std::string_view key, std::string_view value) = 0;

    virtual void remove(const WriteOptions& options, std::string_view key) = 0;

    virtual auto get(const ReadOptions& options, std::string_view key) -> std::optional<std::string> = 0;

    virtual void write(const WriteBatch& batch) = 0;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_DATABASE_H
