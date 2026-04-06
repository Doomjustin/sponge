#ifndef SPONGE_LEVELDB_BLOCK_H
#define SPONGE_LEVELDB_BLOCK_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace spg::leveldb {

class BlockBuilder {
public:
    explicit BlockBuilder(size_t restart_interval = 16);

    BlockBuilder(const BlockBuilder&) = delete;
    auto operator=(const BlockBuilder&) -> BlockBuilder& = delete;

    BlockBuilder(BlockBuilder&&) = default;
    auto operator=(BlockBuilder&&) -> BlockBuilder& = default;

    ~BlockBuilder() = default;

    void add(std::string_view key, std::string_view value);

    void reset();

    [[nodiscard]]
    auto build() noexcept -> std::string_view;

    [[nodiscard]]
    constexpr auto estimate_size() const noexcept -> size_t
    {
        // buffer_ 中的内容 + restarts_ 中的内容 + restarts_ 的个数
        return buffer_.size() + restarts_.size() * sizeof(uint32_t) + sizeof(uint32_t);
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        return buffer_.empty();
    }

private:
    size_t restart_interval_;
    std::string buffer_;
    std::string last_key_;
    std::vector<uint32_t> restarts_;
    size_t counter_ = 0;
    bool is_finished_ = false;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_BLOCK_H
