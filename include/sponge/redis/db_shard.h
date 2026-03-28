#ifndef SPONGE_REDIS_DB_SHARD_H
#define SPONGE_REDIS_DB_SHARD_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

#include <sponge/hash.h>

#include "dash_table.h"
#include "ttl_manager.h"

namespace spg::redis {

// 只是一个逻辑分类，不决定实际的存储方式
enum class ValueType : uint8_t { 
    NotExist, 
    String, 
    HashTable, /* List, SortedSet, ZSet */ 
};

struct AsStringT {};
struct AsIntegralT {};
struct AsListT {};

inline constexpr AsStringT as_string{};
inline constexpr AsIntegralT as_integral{};
inline constexpr AsListT as_list{};

// 支持的类型
// string
// int
// hash
// list
// 需要先对key进行hash，定位到具体的shard，再在shard内部进行操作
class DBShard {
public:
    using String = std::pmr::string;
    using Integral = std::int64_t;
    // using HashTable = std::pmr::unordered_map<String, String>;
    // using Flathash = std::pmr::vector<std::pair<String, String>>;
    // using List = std::pmr::list<String>;
    using Key = String;
    using Value = std::variant<String, Integral>;
    using Size = size_t;
    using MemoryResource = std::pmr::memory_resource;
    using MilliSeconds = TTLManager::Milliseconds;

    explicit DBShard(MemoryResource* resource);

    auto get(std::string_view key, AsStringT t) -> std::optional<std::string_view>;

    auto get(std::string_view key, AsIntegralT t) -> std::optional<Integral>;

    void set(std::string_view key, std::string_view value);

    void set(std::string_view key, Integral value);

    auto erase(std::string_view key) -> bool;

    auto holds(std::string_view key, AsStringT t) -> bool;

    auto holds(std::string_view key, AsIntegralT t) -> bool;

    auto exists(std::string_view key) -> bool;

    auto type_of(std::string_view key) -> ValueType;

    auto expire(std::string_view key, MilliSeconds ttl) -> bool;

    auto persist(std::string_view key) -> bool;

    // 返回剩余TTL，单位毫秒
    // 0: 表示已过期
    // -1：持久化
    // 不存在的key返回std::nullopt
    auto ttl(std::string_view key) -> std::optional<MilliSeconds>;

    [[nodiscard]]
    constexpr auto size() const noexcept -> Size
    {
        return tables_.size();
    }

    [[nodiscard]]
    constexpr auto empty() const noexcept -> bool
    {
        return tables_.empty();
    }

private:
    struct Entry {
        Value value;
        std::int64_t expire_at;
    };

    MemoryResource* resource_;
    DashTable<Entry> tables_;
    TTLManager ttl_manager_;

    auto erase_if_expired(std::string_view key) -> bool;
};

} // namespace spg::redis

#endif // SPONGE_REDIS_DB_SHARD_H
