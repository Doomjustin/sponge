#include "block.h"

#include <cassert>

#include "coding.h"

namespace spg::leveldb {

Block::Block(ByteView data)
  : data_{ data }
{
    if (data_.size() >= HEADER_SIZE) {
        restart_point_count_ = fixed::read<uint32_t>(data_.first(data_.size() - HEADER_SIZE));
        restart_point_offset_ = data_.size() - (1 + restart_point_count_) * HEADER_SIZE;
    }
}

[[nodiscard]]
auto Block::restart_point(size_t index) const noexcept -> uint32_t
{
    assert(index < restart_point_count_);
    auto offset_view = data_.subspan(restart_point_offset_ + index * HEADER_SIZE, HEADER_SIZE);
    return fixed::read<uint32_t>(offset_view);
}


Block::Iterator::Iterator(Block& block)
  : block_{ block }
{}

void Block::Iterator::seek(ByteView target) 
{}


BlockBuilder::BlockBuilder(int restart_interval)
  : restart_interval_{ restart_interval }
{
    restart_points_.push_back(0); // 第一个重启点在 block 的开头
}

void BlockBuilder::append(ByteView key, ByteView value)
{
    size_t shared_prefix = 0;
    if (counter_ < restart_interval_) {
        shared_prefix = compute_shared_prefix(last_key_, key);
    }
    else {
        // 达到重启点，强制不使用前缀压缩
        restart_points_.push_back(static_cast<uint32_t>(buffer_.size()));
        counter_ = 0;
    }

    auto non_shared_key = key.subspan(shared_prefix);

    varint::append(buffer_, shared_prefix);
    varint::append(buffer_, non_shared_key.size());
    varint::append(buffer_, value.size());

    buffer_.insert(buffer_.end(), non_shared_key.begin(), non_shared_key.end());
    buffer_.insert(buffer_.end(), value.begin(), value.end());

    last_key_.resize(shared_prefix);
    last_key_.insert(last_key_.end(), non_shared_key.begin(), non_shared_key.end());
    ++counter_;
}

auto BlockBuilder::finish() -> ByteView
{
    // 写入重启点列表
    for (auto offset : restart_points_)
        fixed::append(buffer_, offset);

    // 写入重启点数量
    fixed::append(buffer_, restart_points_.size());

    return ByteView{ buffer_ };
}

void BlockBuilder::reset()
{
    buffer_.clear();
    restart_points_.clear();
    restart_points_.push_back(0); // 第一个重启点在 block 的开头
    counter_ = 0;
    last_key_.clear();
}

auto BlockBuilder::compute_shared_prefix(ByteView lhs, ByteView rhs) -> size_t
{
    auto [lhs_it, _] = std::ranges::mismatch(lhs, rhs);
    return std::distance(lhs.begin(), lhs_it);
}

} // namespace spg::leveldb
