#include <sponge/leveldb/internal_key.h>

#include <catch2/catch_test_macros.hpp>

using namespace spg::leveldb;

TEST_CASE("在构造 InternalKey 时，encode 返回的内容应包含原始 key", "[leveldb][internal_key]")
{
    InternalKey ik{ "foo", SequenceNumber{ 1UL }, ValueType::Value };

    const auto encoded = ik.encode();

    REQUIRE(encoded.starts_with("foo"));
}

TEST_CASE("在构造 InternalKey 时，encode 返回长度应为 key 长度加 8 字节", "[leveldb][internal_key]")
{
    InternalKey ik{ "hello", SequenceNumber{ 1UL }, ValueType::Value };

    REQUIRE(ik.encode().size() == 5 + sizeof(uint64_t));
}

TEST_CASE("在 view 时，应该正确还原 key", "[leveldb][internal_key]")
{
    InternalKey ik{ "mykey", SequenceNumber{ 42UL }, ValueType::Value };

    const auto v = ik.view();

    REQUIRE(v.key == "mykey");
}

TEST_CASE("在 view 时，应该正确还原 SequenceNumber", "[leveldb][internal_key]")
{
    InternalKey ik{ "mykey", SequenceNumber{ 42UL }, ValueType::Value };

    const auto v = ik.view();

    REQUIRE(v.number == SequenceNumber{ 42UL });
}

TEST_CASE("在 view 时，应该正确还原 ValueType::Value", "[leveldb][internal_key]")
{
    InternalKey ik{ "mykey", SequenceNumber{ 1UL }, ValueType::Value };

    const auto v = ik.view();

    REQUIRE(v.type == ValueType::Value);
}

TEST_CASE("在 view 时，应该正确还原 ValueType::Deletion", "[leveldb][internal_key]")
{
    InternalKey ik{ "mykey", SequenceNumber{ 1UL }, ValueType::Deletion };

    const auto v = ik.view();

    REQUIRE(v.type == ValueType::Deletion);
}

TEST_CASE("在 view 时，SequenceNumber 为 0 应该正确还原", "[leveldb][internal_key]")
{
    InternalKey ik{ "k", SequenceNumber{ 0UL }, ValueType::Value };

    const auto v = ik.view();

    REQUIRE(v.number == SequenceNumber{ 0UL });
}

TEST_CASE("在 view 时，SequenceNumber 为最大值应该正确还原", "[leveldb][internal_key]")
{
    constexpr auto max_seq = (uint64_t{ 1 } << 56) - 1;  // 高 56 位最大值（低 8 位留给 type）
    InternalKey ik{ "k", SequenceNumber{ max_seq }, ValueType::Value };

    const auto v = ik.view();

    REQUIRE(v.number == SequenceNumber{ max_seq });
}

TEST_CASE("在 key 为空字符串时，encode 长度应为 8 字节", "[leveldb][internal_key]")
{
    InternalKey ik{ "", SequenceNumber{ 1UL }, ValueType::Value };

    REQUIRE(ik.encode().size() == sizeof(uint64_t));
}

TEST_CASE("在 key 为空字符串时，view 应该正确还原空 key", "[leveldb][internal_key]")
{
    InternalKey ik{ "", SequenceNumber{ 1UL }, ValueType::Value };

    const auto v = ik.view();

    REQUIRE(v.key.empty());
}

TEST_CASE("在两次构造相同内容的 InternalKey 时，encode 结果应该一致", "[leveldb][internal_key]")
{
    InternalKey a{ "same", SequenceNumber{ 99UL }, ValueType::Value };
    InternalKey b{ "same", SequenceNumber{ 99UL }, ValueType::Value };

    REQUIRE(a.encode() == b.encode());
}

TEST_CASE("在构造不同 SequenceNumber 的 InternalKey 时，encode 结果应该不同", "[leveldb][internal_key]")
{
    InternalKey a{ "key", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 2UL }, ValueType::Value };

    REQUIRE(a.encode() != b.encode());
}

TEST_CASE("在构造不同 ValueType 的 InternalKey 时，encode 结果应该不同", "[leveldb][internal_key]")
{
    InternalKey a{ "key", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 1UL }, ValueType::Deletion };

    REQUIRE(a.encode() != b.encode());
}

TEST_CASE("在比较相同 InternalKey 时，应该返回 equal", "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "key", SequenceNumber{ 10UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 10UL }, ValueType::Value };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) == 0);
}
TEST_CASE("在 user_key 字典序 lhs 小于 rhs 时，应该返回 less", "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "apple", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKey b{ "banana", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) < 0);
}
TEST_CASE("在 user_key 字典序 lhs 大于 rhs 时，应该返回 greater", "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "zebra", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKey b{ "apple", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) > 0);
}
TEST_CASE("在 user_key 相同且 lhs SequenceNumber 更大时，应该返回 less（新版本排前）",
          "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "key", SequenceNumber{ 20UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 10UL }, ValueType::Value };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) < 0);
}
TEST_CASE("在 user_key 相同且 lhs SequenceNumber 更小时，应该返回 greater（旧版本排后）",
          "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "key", SequenceNumber{ 10UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 20UL }, ValueType::Value };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) > 0);
}
TEST_CASE("在 user_key 相同且 SequenceNumber 相同但 ValueType 不同时，应该不相等",
          "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "key", SequenceNumber{ 5UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 5UL }, ValueType::Deletion };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) != 0);
}
TEST_CASE("在 user_key 排序不同时，SequenceNumber 不影响 user_key 层级的比较结果",
          "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "apple", SequenceNumber{ 1UL }, ValueType::Value };
    InternalKey b{ "banana", SequenceNumber{ 100UL }, ValueType::Value };
    InternalKeyComparator cmp;
    REQUIRE(cmp(a.encode(), b.encode()) < 0);
}
TEST_CASE("在比较结果满足反对称性时，交换参数应该翻转符号", "[leveldb][internal_key_comparator]")
{
    InternalKey a{ "key", SequenceNumber{ 20UL }, ValueType::Value };
    InternalKey b{ "key", SequenceNumber{ 10UL }, ValueType::Value };
    InternalKeyComparator cmp;
    const auto ab = cmp(a.encode(), b.encode());
    const auto ba = cmp(b.encode(), a.encode());
    REQUIRE((ab < 0) == (ba > 0));
}
