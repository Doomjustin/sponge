#include "block.h"

#include <cassert>
#include <iterator>

#include <sponge/coding.h>

namespace spg::leveldb {

BlockBuilder::BlockBuilder(size_t restart_interval)
  : restart_interval_(restart_interval) 
{
    restarts_.push_back(0); // 第一个 restart point 的 offset 是 0
}

void BlockBuilder::add(std::string_view key, std::string_view value)
{
    assert(!is_finished_);
    assert(counter_ <= restart_interval_);
    assert(buffer_.empty() || key > last_key_);

    size_t shared = 0;

    if (counter_ < restart_interval_) {
        const auto min_length = std::min(last_key_.size(), key.size());
        while (shared < min_length && last_key_[shared] == key[shared])
            ++shared;
    }
    else {
        restarts_.push_back(static_cast<uint32_t>(buffer_.size()));
        counter_ = 0;
    }

    const auto non_shared = key.size() - shared;

    auto iter = std::back_inserter(buffer_);
    iter = varint::encode(iter, static_cast<uint32_t>(shared));
    iter = varint::encode(iter, static_cast<uint32_t>(non_shared));
    iter = varint::encode(iter, static_cast<uint32_t>(value.size()));

    buffer_.append(key.substr(shared));
    buffer_.append(value);

    last_key_.resize(shared);
    last_key_.append(key.substr(shared));
    ++counter_;
}

void BlockBuilder::reset() 
{
    buffer_.clear();
    last_key_.clear();
    restarts_.clear();
    restarts_.push_back(0);
    counter_ = 0;
    is_finished_ = false;
}

auto BlockBuilder::build() noexcept -> std::string_view
{
    assert(!is_finished_);

    auto iter = std::back_inserter(buffer_);
    for (const auto offset : restarts_)
        iter = fixed::encode(iter, offset);

    fixed::encode(iter, static_cast<uint32_t>(restarts_.size()));
    is_finished_ = true;

    return buffer_;
}

} // namespace spg::leveldb
