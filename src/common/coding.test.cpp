#include <sponge/coding.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

// ── 辅助 ──────────────────────────────────────────────────────────────────────

template<std::unsigned_integral Int>
static auto roundtrip(Int value) -> Int
{
    std::vector<std::byte> buf;
    varint::encode(std::back_inserter(buf), value);
    auto [decoded, _] = varint::decode<decltype(buf.begin()), Int>(buf.begin());
    return decoded;
}

template<std::unsigned_integral Int>
static auto encoded_bytes(Int value) -> std::vector<std::byte>
{
    std::vector<std::byte> buf;
    varint::encode(std::back_inserter(buf), value);
    return buf;
}

// ── 编码字节数 ────────────────────────────────────────────────────────────────

TEST_CASE("varint encode — 单字节 (0~127)", "[varint]")
{
    CHECK(encoded_bytes<uint32_t>(0).size() == 1);
    CHECK(encoded_bytes<uint32_t>(1).size() == 1);
    CHECK(encoded_bytes<uint32_t>(127).size() == 1);
}

TEST_CASE("varint encode — 双字节 (128~16383)", "[varint]")
{
    CHECK(encoded_bytes<uint32_t>(128).size() == 2);
    CHECK(encoded_bytes<uint32_t>(16383).size() == 2);
}

TEST_CASE("varint encode — 三字节 (16384~2097151)", "[varint]")
{
    CHECK(encoded_bytes<uint32_t>(16384).size() == 3);
    CHECK(encoded_bytes<uint32_t>(2097151).size() == 3);
}

TEST_CASE("varint encode — uint32_t 最大值 (5 字节)", "[varint]")
{
    CHECK(encoded_bytes<uint32_t>(std::numeric_limits<uint32_t>::max()).size() == 5);
}

TEST_CASE("varint encode — uint64_t 最大值 (10 字节)", "[varint]")
{
    CHECK(encoded_bytes<uint64_t>(std::numeric_limits<uint64_t>::max()).size() == 10);
}

// ── 编码结构：continuation bit ────────────────────────────────────────────────

TEST_CASE("varint encode — 128 的字节布局", "[varint]")
{
    // 128 = 0b10000000 → varint: [0x80, 0x01]
    auto buf = encoded_bytes<uint32_t>(128);
    REQUIRE(buf.size() == 2);
    CHECK(buf[0] == std::byte{ 0x80 }); // 低 7 位 = 0, continuation bit = 1
    CHECK(buf[1] == std::byte{ 0x01 }); // 高位 = 1, continuation bit = 0
}

TEST_CASE("varint encode — 300 的字节布局", "[varint]")
{
    // 300 = 0b1_0010_1100 → varint: [0xAC, 0x02]
    auto buf = encoded_bytes<uint32_t>(300);
    REQUIRE(buf.size() == 2);
    CHECK(buf[0] == std::byte{ 0xAC }); // (300 & 0x7F) | 0x80 = 0x2C | 0x80
    CHECK(buf[1] == std::byte{ 0x02 }); // 300 >> 7 = 2
}

// ── 解码往返 ──────────────────────────────────────────────────────────────────

TEST_CASE("varint roundtrip — 零", "[varint]")
{
    CHECK(roundtrip<uint32_t>(0) == 0);
    CHECK(roundtrip<uint64_t>(0) == 0);
}

TEST_CASE("varint roundtrip — 单字节边界", "[varint]")
{
    CHECK(roundtrip<uint32_t>(1) == 1);
    CHECK(roundtrip<uint32_t>(127) == 127);
}

TEST_CASE("varint roundtrip — 多字节边界", "[varint]")
{
    CHECK(roundtrip<uint32_t>(128) == 128);
    CHECK(roundtrip<uint32_t>(16383) == 16383);
    CHECK(roundtrip<uint32_t>(16384) == 16384);
}

TEST_CASE("varint roundtrip — uint32_t 极值", "[varint]")
{
    CHECK(roundtrip<uint32_t>(std::numeric_limits<uint32_t>::max()) == std::numeric_limits<uint32_t>::max());
}

TEST_CASE("varint roundtrip — uint64_t 极值", "[varint]")
{
    CHECK(roundtrip<uint64_t>(std::numeric_limits<uint64_t>::max()) == std::numeric_limits<uint64_t>::max());
}

// ── decode 返回正确的迭代器位置 ───────────────────────────────────────────────

TEST_CASE("varint decode — 返回迭代器指向下一个字节", "[varint]")
{
    // 连续编码两个值，解码第一个后迭代器应指向第二个的起始位置
    std::vector<std::byte> buf;
    varint::encode(std::back_inserter(buf), uint32_t{ 300 });
    varint::encode(std::back_inserter(buf), uint32_t{ 42 });

    auto [val1, iter1] = varint::decode<decltype(buf.begin()), uint32_t>(buf.begin());
    CHECK(val1 == 300);

    auto [val2, iter2] = varint::decode<decltype(buf.begin()), uint32_t>(iter1);
    CHECK(val2 == 42);
    CHECK(iter2 == buf.end());
}

TEST_CASE("varint decode — 单字节值迭代器前进一位", "[varint]")
{
    std::vector<std::byte> buf;
    varint::encode(std::back_inserter(buf), uint32_t{ 1 });
    buf.push_back(std::byte{ 0xFF }); // 哨兵

    auto [val, iter] = varint::decode<decltype(buf.begin()), uint32_t>(buf.begin());
    CHECK(val == 1);
    CHECK(iter == buf.begin() + 1);
}
