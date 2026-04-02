#include "internal_key.h"

#include <catch2/catch_test_macros.hpp>

#include <sponge/coding.h>

using namespace spg::leveldb;
using spg::fixed;

// 将 user_key + tag 编码为一个 internal_key 字符串
static auto make_encoded(const std::string_view user_key, const uint64_t seqno, const ValueType type) -> std::string
{
    const auto tag = internal_key::encode_tag(SequenceNumber{ seqno }, type);
    std::string result{ user_key };
    result.resize(user_key.size() + sizeof(uint64_t));
    fixed::encode(result.begin() + static_cast<std::ptrdiff_t>(user_key.size()), tag);
    return result;
}

// ── key_size ──────────────────────────────────────────────────────────────────

TEST_CASE("key_size 应返回 internal_key 长度减去 8 字节", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("foo", 1, ValueType::Value);

    REQUIRE(internal_key::key_size(encoded) == 3);
}

TEST_CASE("key_size 在 user key 为空时应返回 0", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("", 1, ValueType::Value);

    REQUIRE(internal_key::key_size(encoded) == 0);
}

// ── extract_user_key ──────────────────────────────────────────────────────────

TEST_CASE("extract_user_key 应返回原始 user key", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("hello", 42, ValueType::Value);

    REQUIRE(internal_key::extract_user_key(encoded) == "hello");
}

TEST_CASE("extract_user_key 在 user key 为空时应返回空 string_view", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("", 1, ValueType::Value);

    REQUIRE(internal_key::extract_user_key(encoded).empty());
}

// ── encode_tag / extract_packed_number roundtrip ──────────────────────────────

TEST_CASE("encode_tag 与 extract_packed_number 应构成 roundtrip", "[leveldb][internal_key]")
{
    const auto tag     = internal_key::encode_tag(SequenceNumber{ 99 }, ValueType::Deletion);
    const auto encoded = make_encoded("k", 99, ValueType::Deletion);

    REQUIRE(internal_key::extract_tag(encoded) == tag);
}

// ── decode_tag ────────────────────────────────────────────────────────────────

TEST_CASE("decode_tag 应正确还原 user key", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("mykey", 42, ValueType::Value);

    REQUIRE(internal_key::decode_tag(encoded).key == "mykey");
}

TEST_CASE("decode_tag 应正确还原 SequenceNumber", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("mykey", 42, ValueType::Value);

    REQUIRE(internal_key::decode_tag(encoded).number == SequenceNumber{ 42 });
}

TEST_CASE("decode_tag 应正确还原 ValueType::Value", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("k", 1, ValueType::Value);

    REQUIRE(internal_key::decode_tag(encoded).type == ValueType::Value);
}

TEST_CASE("decode_tag 应正确还原 ValueType::Deletion", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("k", 1, ValueType::Deletion);

    REQUIRE(internal_key::decode_tag(encoded).type == ValueType::Deletion);
}

TEST_CASE("decode_tag 在 SequenceNumber 为 0 时应正确还原", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("k", 0, ValueType::Value);

    REQUIRE(internal_key::decode_tag(encoded).number == SequenceNumber{ 0 });
}

TEST_CASE("decode_tag 在 SequenceNumber 为最大 56 位值时应正确还原", "[leveldb][internal_key]")
{
    constexpr uint64_t max_seq = (uint64_t{ 1 } << 56) - 1;
    const auto encoded = make_encoded("k", max_seq, ValueType::Value);

    REQUIRE(internal_key::decode_tag(encoded).number == SequenceNumber{ max_seq });
}

TEST_CASE("decode_tag 在 user key 为空时应还原空 key", "[leveldb][internal_key]")
{
    const auto encoded = make_encoded("", 1, ValueType::Value);

    REQUIRE(internal_key::decode_tag(encoded).key.empty());
}

