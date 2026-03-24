#ifndef SPONGE_LEVELDB_OPTIONS_H
#define SPONGE_LEVELDB_OPTIONS_H

#include <cstddef>

namespace spg::leveldb {

struct Options {
    bool create_if_missing = false;
    bool error_if_exists = false;
    bool paranoid_checks = false;

    size_t write_buffer_size = 4 * 1024 * 1024;
    int max_open_files = 1000;
    size_t block_cache_size = 4 * 1024;
};

struct ReadOptions {
    bool verify_checksums = false;
    bool fill_cache = true;
    // snapshot = nullptr;
};

struct WriteOptions {
    bool sync = false;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_OPTIONS_H
