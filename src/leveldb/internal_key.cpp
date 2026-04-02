#include "internal_key.h"

#include <sponge/coding.h>

namespace spg::leveldb {
    
auto internal_key::key_size(const std::string_view internal_key) -> size_t
{
    assert(internal_key.size() >= PACKED_NUMBER_SIZE);
    return internal_key.size() - PACKED_NUMBER_SIZE;
}

auto internal_key::extract_user_key(const std::string_view internal_key) -> std::string_view
{
    return internal_key.substr(0, key_size(internal_key));
}

auto internal_key::extract_tag(const std::string_view internal_key) -> uint64_t
{
    const auto packed_number_view = internal_key.substr(key_size(internal_key));
    auto [packed_number, _] = fixed::decode<uint64_t>(packed_number_view.begin());
    return packed_number;
}

auto internal_key::encode_tag(SequenceNumber number, const ValueType type) -> uint64_t
{
    return pack(number.get(), pack_bytes<1>(std::to_underlying(type)));
}

auto internal_key::decode_tag(const std::string_view internal_key) -> View
{
    const auto key = internal_key.substr(0, key_size(internal_key));
    const auto packed_number_view = internal_key.substr(key_size(internal_key));

    auto [packed_number, _] = fixed::decode<uint64_t>(packed_number_view.begin());
    auto [type_raw, sequence_number] = unpack<uint8_t, 1>(packed_number);

    return { .key=key, .number=SequenceNumber{ sequence_number }, .type=static_cast<ValueType>(type_raw) };
}

} // namespace spg::leveldb
