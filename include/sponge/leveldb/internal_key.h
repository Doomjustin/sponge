#ifndef SPONGE_LEVELDB_INTERNAL_KEY_H
#define SPONGE_LEVELDB_INTERNAL_KEY_H

#include <cassert>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

#include "formats.h"

namespace spg::leveldb {

class InternalKey {
public:
    struct View {
        std::string_view key;
        SequenceNumber number{ uint64_t{} };
        ValueType type = ValueType::Value;

        View() = default;

        View(const std::string_view key, const SequenceNumber number, const ValueType type)
          : key{ key },
            number{ number },
            type{ type }
        {}
    };

    InternalKey(const std::string_view key, const SequenceNumber number, const ValueType type);

    [[nodiscard]]
    auto view() const noexcept -> View
    {
        return decode_tag(rep_);
    }

    [[nodiscard]]
    auto encode() const noexcept -> std::string_view
    {
        return rep_;
    }

    static auto extract_user_key(const std::string_view internal_key) -> std::string_view
    {
        return internal_key.substr(0, key_size(internal_key));
    }

    static auto extract_packed_number(const std::string_view internal_key) -> uint64_t;

private:
    static constexpr size_t PACKED_NUMBER_SIZE = sizeof(uint64_t);

    std::string rep_;

    static auto key_size(const std::string_view internal_key) -> size_t
    {
        assert(internal_key.size() >= PACKED_NUMBER_SIZE);
        return internal_key.size() - PACKED_NUMBER_SIZE;
    }

    static auto encode_tag(SequenceNumber number, const ValueType type) -> uint64_t;

    static auto decode_tag(const std::string_view internal_key) -> View;
};


class InternalKeyComparator {
public:
    // 满足 SkipList 约定：返回负/零/正整数
    // 排序规则：user_key 升序；相同 user_key 按 SequenceNumber 降序（新版本优先）
    auto operator()(const std::string_view lhs, const std::string_view rhs) const noexcept -> std::strong_ordering;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_INTERNAL_KEY_H
