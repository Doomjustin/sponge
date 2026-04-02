#ifndef SPONGE_LEVELDB_INTERNAL_KEY_H
#define SPONGE_LEVELDB_INTERNAL_KEY_H

#include <cassert>
#include <cstdint>
#include <string_view>

#include <sponge/leveldb/formats.h>

namespace spg::leveldb {

/*
 * | <------------------- N 字节 -------------------> | <------ 8 字节 ------> |
 * +------------------------------------------------+------------------------+
 * |                  User Key                      |       Packed Tag       |
 * +------------------------------------------------+------------------------+
 */
struct internal_key {
    struct View {
        std::string_view key;
        SequenceNumber number{ 0 };
        ValueType type = ValueType::Value;
    };

    static constexpr size_t PACKED_NUMBER_SIZE = sizeof(uint64_t);

    internal_key() = delete;

    static auto key_size(const std::string_view internal_key) -> size_t;

    static auto extract_user_key(const std::string_view internal_key) -> std::string_view;

    static auto extract_tag(const std::string_view internal_key) -> uint64_t;

    static auto encode_tag(SequenceNumber number, const ValueType type) -> uint64_t;

    static auto decode_tag(const std::string_view internal_key) -> View;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_INTERNAL_KEY_H
