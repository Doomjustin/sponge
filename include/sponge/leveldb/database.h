#ifndef SPONGE_LEVELDB_DATABASE_H
#define SPONGE_LEVELDB_DATABASE_H

#include <expected>
#include <memory>

#include "iterator.h"
#include "options.h"
#include "status.h"

namespace spg::leveldb {

class WriteBatch;

class Snapshot;

struct DB {
    virtual ~DB() = default;

    virtual auto put(const WriteOptions& options, std::string_view key, std::string_view value) -> std::expected<void, Status> = 0;

    virtual auto remove(const WriteOptions& options, std::string_view key) -> std::expected<void, Status> = 0;

    virtual auto get(const ReadOptions& options, std::string_view key) -> std::expected<std::string, Status> = 0;

    virtual auto write(const WriteBatch& batch) -> std::expected<void, Status> = 0;

    virtual auto iterator(const ReadOptions& options) -> std::unique_ptr<Iterator> = 0;

    virtual auto snapshot() -> const Snapshot* = 0;

    virtual void release_snapshot(const Snapshot* snapshot) = 0;

    static auto open(const Options& options, std::string_view db_path) -> std::expected<std::unique_ptr<DB>, Status>;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_DATABASE_H
