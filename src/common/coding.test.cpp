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

// ── varint::length() ──────────────────────────────────────────────────────────

TEST_CASE("varint length — 单字节值", "[varint]")
{
    CHECK(varint::length(uint32_t{ 0 }) == 0);
    CHECK(varint::length(uint32_t{ 1 }) == 0);
    CHECK(varint::length(uint32_t{ 127 }) == 0);
}

TEST_CASE("varint length — 多字节值", "[varint]")
{
    CHECK(varint::length(uint32_t{ 128 }) == 1);
    CHECK(varint::length(uint32_t{ 16383 }) == 1);
    CHECK(varint::length(uint32_t{ 16384 }) == 2);
}

TEST_CASE("varint length — 极值", "[varint]")
{
    const auto max32 = std::numeric_limits<uint32_t>::max();
    const auto max64 = std::numeric_limits<uint64_t>::max();
    CHECK(varint::length(max32) == 4);
    CHECK(varint::length(max64) == 9);
}

// ── varint 与 char 迭代器 ───────────────────────────────────────────────────

TEST_CASE("varint encode/decode — char 迭代器", "[varint]")
{
    std::string buf;
    varint::encode(std::back_inserter(buf), uint32_t{ 300 });
    
    auto [decoded, _] = varint::decode<uint32_t>(buf.begin());
    CHECK(decoded == 300);
}

TEST_CASE("varint roundtrip — char 迭代器多值", "[varint]")
{
    std::string buf;
    const uint32_t val1 = 100;
    const uint32_t val2 = 20000;
    
    varint::encode(std::back_inserter(buf), val1);
    varint::encode(std::back_inserter(buf), val2);
    
    auto [dec1, iter1] = varint::decode<uint32_t>(buf.begin());
    auto [dec2, iter2] = varint::decode<uint32_t>(iter1);
    
    CHECK(dec1 == val1);
    CHECK(dec2 == val2);
}

// ── fixed 编码与解码 ────────────────────────────────────────────────────────

TEST_CASE("fixed encode/decode — uint32_t with std::byte", "[fixed]")
{
    std::vector<std::byte> buf;
    const uint32_t value = 0x12345678;
    
    fixed::encode(std::back_inserter(buf), value);
    REQUIRE(buf.size() == 4);
    
    auto [decoded, _] = fixed::decode<uint32_t>(buf.begin());
    CHECK(decoded == value);
}

TEST_CASE("fixed encode/decode — uint64_t with std::byte", "[fixed]")
{
    std::vector<std::byte> buf;
    const uint64_t value = 0x0123456789ABCDEF;
    
    fixed::encode(std::back_inserter(buf), value);
    REQUIRE(buf.size() == 8);
    
    auto [decoded, _] = fixed::decode<uint64_t>(buf.begin());
    CHECK(decoded == value);
}

TEST_CASE("fixed encode/decode — char 迭代器", "[fixed]")
{
    std::string buf;
    const uint32_t value = 0xDEADBEEF;
    
    fixed::encode(std::back_inserter(buf), value);
    REQUIRE(buf.size() == 4);
    
    auto [decoded, _] = fixed::decode<uint32_t>(buf.c_str());
    CHECK(decoded == value);
}

TEST_CASE("fixed encode — 小端字节顺序", "[fixed]")
{
    std::vector<std::byte> buf;
    fixed::encode(std::back_inserter(buf), uint32_t{ 0x12345678 });
    
    REQUIRE(buf.size() == 4);
    CHECK(buf[0] == std::byte{ 0x78 }); // 最低字节
    CHECK(buf[1] == std::byte{ 0x56 });
    CHECK(buf[2] == std::byte{ 0x34 });
    CHECK(buf[3] == std::byte{ 0x12 }); // 最高字节
}

TEST_CASE("fixed roundtrip — 多个值", "[fixed]")
{
    std::vector<std::byte> buf;
    const uint16_t val1 = 0x1234;
    const uint32_t val2 = 0x56789ABC;
    
    fixed::encode(std::back_inserter(buf), val1);
    fixed::encode(std::back_inserter(buf), val2);
    
    auto [dec1, iter1] = fixed::decode<uint16_t>(buf.begin());
    auto [dec2, iter2] = fixed::decode<uint32_t>(iter1);
    
    CHECK(dec1 == val1);
    CHECK(dec2 == val2);
}

// ── pack/unpack ───────────────────────────────────────────────────────────────

TEST_CASE("pack_bytes — 创建打包值", "[pack]")
{
    auto pb = pack_bytes<2>(0x1234);
    CHECK(pb.value == 0x1234);
}

TEST_CASE("pack — 单字节打包", "[pack]")
{
    // pack 将 0xAB 打包到 0x00 中，结果是 0xAB
    const auto result = pack<uint32_t>(0x00, pack_bytes<1>(0xAB));
    CHECK(result == 0xAB);
}

TEST_CASE("pack — 多字节顺序打包", "[pack]")
{
    // 第一次: pack(0x00, pack_bytes<1>(0x12)) → 0x12
    auto result = pack<uint32_t>(0x00, pack_bytes<1>(0x12));
    // 第二次: pack(0x12, pack_bytes<1>(0x34)) → 0x3412
    result = pack<uint32_t>(result, pack_bytes<1>(0x34));
    // 第三次: pack(0x3412, pack_bytes<1>(0x56)) → 0x563412
    result = pack<uint32_t>(result, pack_bytes<1>(0x56));
    // 第四次: pack(0x563412, pack_bytes<1>(0x78)) → 0x78563412
    result = pack<uint32_t>(result, pack_bytes<1>(0x78));
    
    CHECK(result == 0x78563412);
}

TEST_CASE("unpack — 提取单字节", "[unpack]")
{
    const uint32_t packed = 0xABCDEF12;
    
    auto [byte1, rest1] = unpack<uint8_t, 1>(packed);
    CHECK(byte1 == 0x12);
    CHECK(rest1 == 0xABCDEF);
}

TEST_CASE("unpack — 顺序提取多字节", "[unpack]")
{
    uint32_t packed = 0x78563412;
    
    auto [byte1, rest1] = unpack<uint8_t, 1>(packed);
    CHECK(byte1 == 0x12);
    
    auto [byte2, rest2] = unpack<uint8_t, 1>(rest1);
    CHECK(byte2 == 0x34);
    
    auto [byte3, rest3] = unpack<uint8_t, 1>(rest2);
    CHECK(byte3 == 0x56);
    
    auto [byte4, rest4] = unpack<uint8_t, 1>(rest3);
    CHECK(byte4 == 0x78);
}

TEST_CASE("pack/unpack — 往返", "[pack]")
{
    // 打包四个字节
    auto packed = pack<uint32_t>(0x00, pack_bytes<1>(0x11));
    packed = pack<uint32_t>(packed, pack_bytes<1>(0x22));
    packed = pack<uint32_t>(packed, pack_bytes<1>(0x33));
    packed = pack<uint32_t>(packed, pack_bytes<1>(0x44));
    
    // 解包验证
    auto [b1, r1] = unpack<uint8_t, 1>(packed);
    auto [b2, r2] = unpack<uint8_t, 1>(r1);
    auto [b3, r3] = unpack<uint8_t, 1>(r2);
    auto [b4, r4] = unpack<uint8_t, 1>(r3);
    
    CHECK(b1 == 0x11);
    CHECK(b2 == 0x22);
    CHECK(b3 == 0x33);
    CHECK(b4 == 0x44);
}
