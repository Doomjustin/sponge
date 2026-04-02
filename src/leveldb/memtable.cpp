#include <sponge/leveldb/memtable.h>

#include <cstring>
#include <string_view>

#include <sponge/coding.h>

#include "internal_key.h"

namespace spg::leveldb {

namespace  {

auto extract_internal_key(std::string_view entry) -> std::string_view
{
    auto iter = entry.begin();
    auto [internal_key_size, next] = varint::decode<uint32_t>(iter);
    return std::string_view{ &*next, internal_key_size };
}

auto extract_entry(std::string_view entry) -> std::pair<std::string_view, std::string_view>
{
    auto iter = entry.begin();

    auto [internal_key_size, iter1] = varint::decode<uint32_t>(iter);
    iter = iter1;

    const auto internal_key_view = std::string_view{ &*iter, internal_key_size };
    iter += internal_key_size;

    auto [value_size, iter2] = varint::decode<uint32_t>(iter);
    iter = iter2;

    const auto value_view = std::string_view{ &*iter, value_size };

    return { internal_key_view, value_view };
}

} // namespace

MemTable::MemTable(gsl::not_null<MemoryResource*> mem_resource)
  : resource_{ mem_resource },
    skip_list_{ PackedKeyComparator{}, mem_resource }
{}

void MemTable::add(SequenceNumber sequence_number, ValueType type, std::string_view key, std::string_view value) noexcept
{
    const auto internal_key_size = key.size() + internal_key::PACKED_NUMBER_SIZE;
    const auto entry_size = varint::length(static_cast<KeySizeType>(internal_key_size))
                            + internal_key_size
                            + varint::length(static_cast<ValueSizeType>(value.size()))
                            + value.size();

    // Varint32(InternalKey_Length) | InternalKey_Bytes | Varint32(Value_Length) | Value_Bytes
    auto* buffer = static_cast<char*>(resource_->allocate(entry_size, alignof(char)));
    char* ptr = buffer;

    ptr = varint::encode(ptr, static_cast<KeySizeType>(internal_key_size));
    std::memcpy(ptr, key.data(), key.size());
    ptr += key.size();
    ptr = fixed::encode(ptr, internal_key::encode_tag(sequence_number, type));

    ptr = varint::encode(ptr, static_cast<ValueSizeType>(value.size()));
    std::memcpy(ptr, value.data(), value.size());

    // buffer的生命周期由resource管理，所以我们可以安全的传一个string_view到skip_list中
    skip_list_.insert(std::string_view{ buffer, entry_size });
}

auto MemTable::get(std::string_view key, SequenceNumber sequence_number) const noexcept -> ReadResult
{
    Query query{ .key = key, .sequence_number = sequence_number };
    auto iter = skip_list_.find(query);
    if (!iter.is_valid())
        return NotFound{};

    auto [internal_key_view, value_view] = extract_entry(iter.key());

    auto user_key = internal_key::extract_user_key(internal_key_view);
    if (user_key != key)
        return NotFound{};

    auto tag = internal_key::extract_tag(internal_key_view);
    auto [type, _] = unpack<uint8_t, 1>(tag);
    if (type == std::to_underlying(ValueType::Deletion))
        return Deleted{};

    return value_view;
}


auto MemTable::PackedKeyComparator::operator()(const std::string_view lhs, const std::string_view rhs) const noexcept -> std::strong_ordering
{
    const auto lhs_internal_key = extract_internal_key(lhs);
    const auto rhs_internal_key = extract_internal_key(rhs);

    auto lhs_user_key = internal_key::extract_user_key(lhs_internal_key);
    auto rhs_user_key = internal_key::extract_user_key(rhs_internal_key);
    if (const auto cmp = lhs_user_key <=> rhs_user_key; cmp != 0)
        return cmp;

    auto lhs_packed_number = internal_key::extract_tag(lhs_internal_key);
    auto rhs_packed_number = internal_key::extract_tag(rhs_internal_key);
    return rhs_packed_number <=> lhs_packed_number;
}

auto MemTable::PackedKeyComparator::operator()(const std::string_view key, const Query& query) const noexcept -> std::strong_ordering
{
    const auto internal_key_view = extract_internal_key(key);
    auto key_user_key = internal_key::extract_user_key(internal_key_view);
    if (const auto cmp = key_user_key <=> query.key; cmp != 0)
        return cmp;

    auto key_packed_number = internal_key::extract_tag(internal_key_view);
    auto query_packed_number = internal_key::encode_tag(query.sequence_number, ValueType::Value);
    return query_packed_number <=> key_packed_number;
}

auto MemTable::PackedKeyComparator::operator()(const Query& query, const std::string_view key) const noexcept -> std::strong_ordering
{
    const auto internal_key_view = extract_internal_key(key);
    auto key_user_key = internal_key::extract_user_key(internal_key_view);
    if (const auto cmp = query.key <=> key_user_key; cmp != 0)
        return cmp;

    auto key_packed_number = internal_key::extract_tag(internal_key_view);
    auto query_packed_number = internal_key::encode_tag(query.sequence_number, ValueType::Value);
    return query_packed_number <=> key_packed_number;
}

} // namespace spg::leveldb
