#include <sponge/leveldb/internal_key.h>

#include <sponge/coding.h>

namespace spg::leveldb {

InternalKey::InternalKey(const std::string_view key, const SequenceNumber number, const ValueType type)
{
    rep_.reserve(key.size() + PACKED_NUMBER_SIZE);
    rep_.append(key);

    const auto packed_number = encode_tag(number, type);
    fixed::encode(std::back_inserter(rep_), packed_number);
}

auto InternalKey::extract_packed_number(const std::string_view internal_key) -> uint64_t
{
    const auto packed_number_view = internal_key.substr(key_size(internal_key));
    auto [packed_number, _] = fixed::decode<uint64_t>(packed_number_view.begin());
    return packed_number;
}

auto InternalKey::encode_tag(SequenceNumber number, const ValueType type) -> uint64_t
{
    return pack(number.get(), pack_bytes<1>(std::to_underlying(type)));
}

auto InternalKey::decode_tag(const std::string_view internal_key) -> View
{
    const auto key = internal_key.substr(0, key_size(internal_key));
    const auto packed_number_view = internal_key.substr(key_size(internal_key));

    auto [packed_number, _] = fixed::decode<uint64_t>(packed_number_view.begin());
    auto [type_raw, sequence_number] = unpack<uint8_t, 1>(packed_number);

    return View{ key, SequenceNumber{ sequence_number }, static_cast<ValueType>(type_raw) };
}


auto InternalKeyComparator::operator()(const std::string_view lhs, const std::string_view rhs) const noexcept -> std::strong_ordering
{
    const auto lhs_key = InternalKey::extract_user_key(lhs);
    const auto rhs_key = InternalKey::extract_user_key(rhs);
    if (const auto cmp = lhs_key <=> rhs_key; cmp != 0)
        return cmp;

    const auto lhs_packed = InternalKey::extract_packed_number(lhs);
    const auto rhs_packed = InternalKey::extract_packed_number(rhs);
    return rhs_packed <=> lhs_packed;
}

} // namespace spg::leveldb
