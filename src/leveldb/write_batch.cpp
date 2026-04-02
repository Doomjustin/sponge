#include <sponge/leveldb/write_batch.h>

#include <iterator>
#include <utility>

#include <sponge/coding.h>

namespace spg::leveldb {

WriteBatch::WriteBatch(gsl::not_null<MemoryResource*> resource)
    : rep_{ resource.get() }
{
    clear();
}

void WriteBatch::clear()
{
    rep_.clear();
    rep_.resize(HEADER_SIZE, '\0');
}

void WriteBatch::put(std::string_view key, std::string_view value)
{
    set_count(count() + 1);
    
    fixed::encode(std::back_inserter(rep_), std::to_underlying(ValueType::Value));

    varint::encode(std::back_inserter(rep_), key.size());
    rep_.append(key);

    varint::encode(std::back_inserter(rep_), value.size());
    rep_.append(value);
}

void WriteBatch::erase(std::string_view key)
{
    set_count(count() + 1);
    
    fixed::encode(std::back_inserter(rep_), std::to_underlying(ValueType::Deletion));

    varint::encode(std::back_inserter(rep_), key.size());
    rep_.append(key);
}

[[nodiscard]]
auto WriteBatch::sequence() const noexcept -> SequenceNumber
{
    auto [number, _] = fixed::decode<SequenceNumberType>(rep_.data());
    return SequenceNumber{ number };
}

void WriteBatch::set_sequence(SequenceNumber new_number)
{
    fixed::encode(rep_.data(), new_number.get());
}

[[nodiscard]]
auto WriteBatch::count() const noexcept -> uint32_t
{
    auto [count, _] = fixed::decode<SizeType>(count_begin());
    return count;
}

void WriteBatch::set_count(uint32_t new_count)
{
    fixed::encode(count_begin(), new_count);
}

} // namespace spg::leveldb
