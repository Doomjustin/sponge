#ifndef SPONGE_LEVELDB_BLOCK_H
#define SPONGE_LEVELDB_BLOCK_H

#include <cstdint>
#include <span>
#include <vector>

namespace spg::leveldb {

class Block {
public:
    using ByteView = std::span<const std::byte>;
    static constexpr size_t HEADER_SIZE = sizeof(uint32_t);

    explicit Block(ByteView data);

    [[nodiscard]]
    constexpr auto size() const noexcept -> size_t
    {
        return data_.size();
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        return data_.empty();
    }

    [[nodiscard]]
    constexpr auto restart_point_count() const noexcept -> uint32_t
    {
        return restart_point_count_;
    }

    [[nodiscard]]
    auto restart_point(size_t index) const noexcept -> uint32_t;

    class Iterator {
    public:
        explicit Iterator(Block& block);

        void seek(ByteView target);

    private:
        Block& block_;
        size_t current_offset_ = 0;
        std::vector<std::byte> current_key_;
        ByteView current_value_;
    };

    auto iterator() -> Iterator { return Iterator{ *this }; }

private:
    ByteView data_;
    uint32_t restart_point_count_ = 0;
    size_t restart_point_offset_ = 0;
};


/**
 * SSTable 的宏观文件切分
 * 一个完整的 .sst 文件在物理上是由尾部向头部解析的。它的宏观结构如下
 * 1. Data Blocks (数据块簇)：占文件体积的 99%。默认按 4KB 大小切分。存放真正的 KV 数据。
 * 2. Filter Block (布隆过滤器块)：存放 Bloom Filter，用于极速判断某个 Key 是否绝对不在这个文件中。
 * 3. Meta Index Block：记录 Filter Block 的偏移量。
 * 4. Index Block (索引块)：记录每一个 Data Block 的最大 Key 和在文件中的偏移量。相当于书的目录。
 * 5. Footer (文件尾)：记录 Index Block 和 Meta Index Block 的偏移量，以及一个魔数用于校验。
 *
 * 读取逻辑：打开文件 -> 读最后 48 字节 (Footer) -> 找到 Index Block -> 二分查找定位到特定的 Data
 * Block -> 把这个 4KB 的 Data Block 读入内存 -> 在 Block 内部继续查找。
 * 由于跳表吐出的数据是严格按字典序递增的，相邻的 Key 通常会有极其漫长的相同前缀。
 * 如果原样存，user:10001: 会被重复存无数次，浪费大量磁盘和内存（Block Cache）。因此，LevelDB
 * 引入了前缀压缩。
 * 但是，前缀压缩带来了一个致命的副作用：无法二分查找了！
 * 为了在“高压缩率”和“二分查找”之间取得平衡，LevelDB 巧妙地引入了 重启点（Restart Points）。
 * 规则很简单：每隔固定的K条记录（默认K=16），我们就强制不使用前缀压缩（即共享前缀长度设为0），
 * 把完整的 Key 原原本本地存下来。 这个记录的位置，就叫一个“重启点”。
 */

/**
 * 每一条 KV 记录（Entry）在 Block 中的二进制格式如下：
 * [共享前缀长度 (Varint32)]
 * [非共享后缀长度 (Varint32)]
 * [Value长度 (Varint32)]
 * [非共享的 Key]
 * [Value]
 */
class BlockBuilder {
public:
    using ByteView = std::span<const std::byte>;

    explicit BlockBuilder(int restart_interval = 16);

    void append(ByteView key, ByteView value);

    [[nodiscard]]
    auto finish() -> ByteView;

    void reset();

    [[nodiscard]]
    constexpr auto current_size_estimate() const -> size_t
    {
        return buffer_.size() + restart_points_.size() * sizeof(uint32_t) + sizeof(uint32_t);
    }

private:
    int restart_interval_;
    int counter_ = 0; // 距离上一个重启点插入的记录数

    std::vector<std::byte> buffer_;
    std::vector<uint32_t> restart_points_;
    std::vector<std::byte> last_key_; // 用于计算共享前缀长度

    static auto compute_shared_prefix(ByteView lhs, ByteView rhs) -> size_t;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_BLOCK_H
