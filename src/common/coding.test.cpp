#include <sponge/coding.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg;

TEST_CASE("在编码单字节值时，应该只输出一个字节", "[coding]")
{
    // 测试单字节编码（值 < 128）
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), uint8_t{42});

    REQUIRE(buffer.size() == 1);
    REQUIRE(buffer[0] == std::byte{42});
}

TEST_CASE("在编码多字节值时，应该输出预期字节序列", "[coding]")
{
    // 测试多字节编码
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), uint16_t{300});

    REQUIRE(buffer.size() == 2);
    // 300 = 0b00010010_1100
    // 低7位: 0x2C (44), 最高位1
    // 高位: 0x02, 最高位0
    REQUIRE(buffer[0] == std::byte{0xAC});
    REQUIRE(buffer[1] == std::byte{0x02});
}

TEST_CASE("在编码零值时，应该输出单个零字节", "[coding]")
{
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), uint32_t{0});

    REQUIRE(buffer.size() == 1);
    REQUIRE(buffer[0] == std::byte{0});
}

TEST_CASE("在编码 uint8 最大值时，应该输出两个字节", "[coding]")
{
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), std::numeric_limits<uint8_t>::max());

    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer[0] == std::byte{0xFF});
    REQUIRE(buffer[1] == std::byte{0x01});
}

TEST_CASE("在编码 uint16 最大值时，应该输出三个字节", "[coding]")
{
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), std::numeric_limits<uint16_t>::max());

    REQUIRE(buffer.size() == 3);
}

TEST_CASE("在编码 uint32 最大值时，应该输出五个字节", "[coding]")
{
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), std::numeric_limits<uint32_t>::max());

    REQUIRE(buffer.size() == 5);
}

TEST_CASE("在编码 uint64 最大值时，应该输出十个字节", "[coding]")
{
    std::vector<std::byte> buffer;
    varint::encode(std::back_inserter(buffer), std::numeric_limits<uint64_t>::max());

    REQUIRE(buffer.size() == 10);
}

TEST_CASE("在解码单字节 varint 时，应该得到正确值与迭代器位置", "[coding]")
{
    std::vector<std::byte> buffer{std::byte{42}};
    auto [value, iter] = varint::decode<uint8_t>(buffer.begin());

    REQUIRE(value == 42);
    REQUIRE(std::distance(buffer.begin(), iter) == 1);
}

TEST_CASE("在解码多字节 varint 时，应该得到正确值与迭代器位置", "[coding]")
{
    std::vector<std::byte> buffer{std::byte{0xAC}, std::byte{0x02}};
    auto [value, iter] = varint::decode<uint16_t>(buffer.begin());

    REQUIRE(value == 300);
    REQUIRE(std::distance(buffer.begin(), iter) == 2);
}

TEST_CASE("在解码零值时，应该得到零", "[coding]")
{
    std::vector<std::byte> buffer{std::byte{0}};
    auto [value, iter] = varint::decode<uint32_t>(buffer.begin());

    REQUIRE(value == 0);
}

TEST_CASE("在解码 uint8 最大值编码结果时，应该还原为 255", "[coding]")
{
    std::vector<std::byte> buffer{ std::byte{0xFF}, std::byte{0x01} };
    auto [value, iter] = varint::decode<uint16_t>(buffer.begin());

    REQUIRE(value == 255);
}

TEST_CASE("在 uint8 全范围往返编码解码时，应该保持数值一致", "[coding]")
{
    for (uint32_t val = 0; val <= 255; ++val) {
        std::vector<std::byte> buffer;
        varint::encode(std::back_inserter(buffer), static_cast<uint8_t>(val));
        auto [decoded, _] = varint::decode<uint8_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在 uint16 边界值往返编码解码时，应该保持数值一致", "[coding]")
{
    const std::vector<uint16_t> values{
        0,
        1,
        127,
        128,
        255,
        256,
        16383,
        16384,
        std::numeric_limits<uint16_t>::max()
    };

    for (auto val : values) {
        std::vector<std::byte> buffer;
        varint::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = varint::decode<uint16_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在 uint32 边界值往返编码解码时，应该保持数值一致", "[coding]")
{
    const std::vector<uint32_t> values{
        0,
        127,
        128,
        16383,
        16384,
        2097151,
        2097152,
        268435455,
        268435456,
        std::numeric_limits<uint32_t>::max()
    };

    for (auto val : values) {
        std::vector<std::byte> buffer;
        varint::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = varint::decode<uint32_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在 uint64 边界值往返编码解码时，应该保持数值一致", "[coding]")
{
    const std::vector<uint64_t> values{
        0,
        127,
        128,
        16383,
        16384,
        2097151,
        2097152,
        268435455,
        268435456,
        34359738367,
        34359738368,
        std::numeric_limits<uint64_t>::max()
    };

    for (auto val : values) {
        std::vector<std::byte> buffer;
        varint::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = varint::decode<uint64_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在使用 char 迭代器编码时，应该得到正确字节内容", "[coding]")
{
    std::vector<char> buffer;
    varint::encode(std::back_inserter(buffer), uint32_t{42});

    REQUIRE(buffer.size() == 1);
    REQUIRE(static_cast<unsigned char>(buffer[0]) == 42);
}

TEST_CASE("在使用 char 迭代器解码时，应该得到正确值与迭代器位置", "[coding]")
{
    std::vector<char> buffer;
    varint::encode(std::back_inserter(buffer), uint16_t{300});

    auto [value, iter] = varint::decode<uint16_t>(buffer.begin());

    REQUIRE(value == 300);
    REQUIRE(std::distance(buffer.begin(), iter) == 2);
}

TEST_CASE("在使用 char 迭代器对 uint16 往返编码解码时，应该保持数值一致", "[coding]")
{
    const std::vector<uint16_t> values{0, 127, 128, 255, 256, 16383, 16384, 65535};

    for (auto val : values) {
        std::vector<char> buffer;
        varint::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = varint::decode<uint16_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在不同数值范围编码时，应该满足预期长度", "[coding]")
{
    // 单字节: 0 ~ 127
    std::vector<std::byte> buf1;
    varint::encode(std::back_inserter(buf1), uint32_t{127});
    REQUIRE(buf1.size() == 1);

    // 双字节: 128 ~ 16383
    std::vector<std::byte> buf2;
    varint::encode(std::back_inserter(buf2), uint32_t{128});
    REQUIRE(buf2.size() == 2);

    std::vector<std::byte> buf3;
    varint::encode(std::back_inserter(buf3), uint32_t{16383});
    REQUIRE(buf3.size() == 2);

    // 三字节: 16384 ~ 2097151
    std::vector<std::byte> buf4;
    varint::encode(std::back_inserter(buf4), uint32_t{16384});
    REQUIRE(buf4.size() == 3);

    std::vector<std::byte> buf5;
    varint::encode(std::back_inserter(buf5), uint32_t{2097151});
    REQUIRE(buf5.size() == 3);

    // 四字节: 2097152 ~ 268435455
    std::vector<std::byte> buf6;
    varint::encode(std::back_inserter(buf6), uint32_t{2097152});
    REQUIRE(buf6.size() == 4);

    // 五字节: 268435456 ~ 4294967295
    std::vector<std::byte> buf7;
    varint::encode(std::back_inserter(buf7), uint32_t{268435456});
    REQUIRE(buf7.size() == 5);
}

TEST_CASE("在连续编码多个值时，应该按顺序写入字节流", "[coding]")
{
    std::vector<std::byte> buffer;

    varint::encode(std::back_inserter(buffer), uint32_t{42});
    varint::encode(std::back_inserter(buffer), uint32_t{300});
    varint::encode(std::back_inserter(buffer), uint32_t{0});

    REQUIRE(buffer.size() == 4); // 1 + 2 + 1 字节
}

TEST_CASE("在连续解码多个值时，应该按顺序恢复所有值", "[coding]")
{
    std::vector<std::byte> buffer;

    varint::encode(std::back_inserter(buffer), uint32_t{42});
    varint::encode(std::back_inserter(buffer), uint32_t{300});
    varint::encode(std::back_inserter(buffer), uint32_t{1000});

    auto iter = buffer.begin();

    auto [v1, iter1] = varint::decode<uint32_t>(iter);
    REQUIRE(v1 == 42);

    auto [v2, iter2] = varint::decode<uint32_t>(iter1);
    REQUIRE(v2 == 300);

    auto [v3, iter3] = varint::decode<uint32_t>(iter2);
    REQUIRE(v3 == 1000);

    REQUIRE(iter3 == buffer.end());
}

TEST_CASE("在编码到 std::string 时，应该写入正确字节", "[coding]")
{
    std::string buffer;
    varint::encode(std::back_inserter(buffer), uint32_t{42});

    REQUIRE(buffer.size() == 1);
    REQUIRE(static_cast<unsigned char>(buffer[0]) == 42);
}

TEST_CASE("在从 std::string 解码时，应该得到正确值与迭代器位置", "[coding]")
{
    std::string buffer;
    varint::encode(std::back_inserter(buffer), uint16_t{300});

    auto [value, iter] = varint::decode<uint16_t>(buffer.begin());

    REQUIRE(value == 300);
    REQUIRE(std::distance(buffer.begin(), iter) == 2);
}

TEST_CASE("在 std::string 中对 uint32 往返编码解码时，应该保持数值一致", "[coding]")
{
    const std::vector<uint32_t> values{0, 1, 127, 128, 255, 256, 16383, 16384, 65535, 1000000};

    for (auto val : values) {
        std::string buffer;
        varint::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = varint::decode<uint32_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在 std::string 中连续编码解码多个值时，应该按顺序完整恢复", "[coding]")
{
    std::string buffer;

    varint::encode(std::back_inserter(buffer), uint32_t{42});
    varint::encode(std::back_inserter(buffer), uint32_t{300});
    varint::encode(std::back_inserter(buffer), uint32_t{255});
    varint::encode(std::back_inserter(buffer), uint32_t{1000000});

    auto iter = buffer.begin();

    auto [v1, iter1] = varint::decode<uint32_t>(iter);
    REQUIRE(v1 == 42);

    auto [v2, iter2] = varint::decode<uint32_t>(iter1);
    REQUIRE(v2 == 300);

    auto [v3, iter3] = varint::decode<uint32_t>(iter2);
    REQUIRE(v3 == 255);

    auto [v4, iter4] = varint::decode<uint32_t>(iter3);
    REQUIRE(v4 == 1000000);

    REQUIRE(iter4 == buffer.end());
}

TEST_CASE("在 std::string 与 vector 编码同一数值时，应该得到一致字节序列", "[coding]")
{
    // 验证 std::string 和 std::vector<char> 编码结果一致
    const std::vector<uint32_t> values{0, 127, 128, 16383, 16384, 268435455};

    for (auto val : values) {
        std::string str_buffer;
        std::vector<char> vec_buffer;

        varint::encode(std::back_inserter(str_buffer), val);
        varint::encode(std::back_inserter(vec_buffer), val);

        REQUIRE(str_buffer.size() == vec_buffer.size());
        for (size_t i = 0; i < str_buffer.size(); ++i) {
            REQUIRE(static_cast<unsigned char>(str_buffer[i]) == static_cast<unsigned char>(vec_buffer[i]));
        }
    }
}

TEST_CASE("在随机 uint32 值往返编码解码时，应该保持数值一致", "[coding]")
{
    // 测试一些随机的 uint32 值
    const std::vector<uint32_t> random_values{
        12345,
        67890,
        1000000,
        999999,
        123456789,
        987654321,
        100000000,
        1,
        std::numeric_limits<uint32_t>::max(),
        std::numeric_limits<uint32_t>::max() - 1
    };

    for (auto val : random_values) {
        std::vector<std::byte> buffer;
        varint::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = varint::decode<uint32_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("在混合类型连续编码解码时，应该按类型顺序正确恢复", "[coding]")
{
    // 在同一个缓冲区中编码不同类型的值
    std::vector<std::byte> buffer;

    varint::encode(std::back_inserter(buffer), uint8_t{42});
    varint::encode(std::back_inserter(buffer), uint32_t{300});
    varint::encode(std::back_inserter(buffer), uint64_t{1000000});

    auto iter = buffer.begin();

    // 按相同类型的顺序解码
    auto [v1, iter1] = varint::decode<uint8_t>(iter);
    REQUIRE(v1 == 42);

    auto [v2, iter2] = varint::decode<uint32_t>(iter1);
    REQUIRE(v2 == 300);

    auto [v3, iter3] = varint::decode<uint64_t>(iter2);
    REQUIRE(v3 == 1000000);

    REQUIRE(iter3 == buffer.end());
}

TEST_CASE("在混合类型边界值编码解码时，应该正确恢复各边界值", "[coding]")
{
    // 使用边界值测试混合类型
    std::vector<std::byte> buffer;

    varint::encode(std::back_inserter(buffer), uint8_t{127});  // max single-byte
    varint::encode(std::back_inserter(buffer), uint16_t{128}); // min two-byte
    varint::encode(std::back_inserter(buffer), uint32_t{16383}); // max two-byte
    varint::encode(std::back_inserter(buffer), uint64_t{16384}); // min three-byte
    varint::encode(std::back_inserter(buffer), uint8_t{0});    // zero

    auto iter = buffer.begin();

    auto [v1, iter1] = varint::decode<uint8_t>(iter);
    REQUIRE(v1 == 127);

    auto [v2, iter2] = varint::decode<uint16_t>(iter1);
    REQUIRE(v2 == 128);

    auto [v3, iter3] = varint::decode<uint32_t>(iter2);
    REQUIRE(v3 == 16383);

    auto [v4, iter4] = varint::decode<uint64_t>(iter3);
    REQUIRE(v4 == 16384);

    auto [v5, iter5] = varint::decode<uint8_t>(iter4);
    REQUIRE(v5 == 0);

    REQUIRE(iter5 == buffer.end());
}

TEST_CASE("在 std::string 中混合类型编码解码时，应该按顺序完整恢复", "[coding]")
{
    // 在 std::string 中编码和解码混合类型
    std::string buffer;

    varint::encode(std::back_inserter(buffer), uint8_t{10});
    varint::encode(std::back_inserter(buffer), uint32_t{100000});
    varint::encode(std::back_inserter(buffer), uint16_t{255});
    varint::encode(std::back_inserter(buffer), uint64_t{999999});

    auto iter = buffer.begin();

    auto [v1, iter1] = varint::decode<uint8_t>(iter);
    REQUIRE(v1 == 10);

    auto [v2, iter2] = varint::decode<uint32_t>(iter1);
    REQUIRE(v2 == 100000);

    auto [v3, iter3] = varint::decode<uint16_t>(iter2);
    REQUIRE(v3 == 255);

    auto [v4, iter4] = varint::decode<uint64_t>(iter3);
    REQUIRE(v4 == 999999);

    REQUIRE(iter4 == buffer.end());
}

TEST_CASE("在 pack 将一个字节拼接到另一个值时，应该正确左移并拼接", "[coding]")
{
    auto result = pack(0x12ULL, pack_bytes<1>(0x34U));
    REQUIRE(result == 0x1234ULL);
}

TEST_CASE("在 pack 连续拼接两个字节时，应该逐级正确组装", "[coding]")
{
    auto step1 = pack(0x12ULL, pack_bytes<1>(0x34U));
    auto step2 = pack(step1, pack_bytes<1>(0x56U));
    REQUIRE(step2 == 0x123456ULL);
}

TEST_CASE("在 pack 拼接时超出 N 字节的值应该被掩码剔除", "[coding]")
{
    // src.value 是 0x1234，但 N=1 时应只取低字节 0x34
    auto result = pack(0x12ULL, pack_bytes<1>(0x1234U));
    REQUIRE(result == 0x1234ULL);
}

TEST_CASE("在 unpack 从拼接值中提取第一个字节时，应该正确解包", "[coding]")
{
    auto [extracted, remaining] = unpack<std::uint32_t, 1, std::uint64_t>(0x1234ULL);
    REQUIRE(extracted == 0x34U);
    REQUIRE(remaining == 0x12ULL);
}

TEST_CASE("在 unpack 连续解包两个字节时，应该逐级正确分离", "[coding]")
{
    auto [first, remainder1] = unpack<std::uint8_t, 1, std::uint64_t>(0x123456ULL);
    REQUIRE(first == 0x56U);
    REQUIRE(remainder1 == 0x1234ULL);

    auto [second, remainder2] = unpack<std::uint8_t, 1, std::uint64_t>(remainder1);
    REQUIRE(second == 0x34U);
    REQUIRE(remainder2 == 0x12ULL);
}

TEST_CASE("在 pack 然后 unpack 时，应该恢复原始值", "[coding]")
{
    const std::uint32_t original = 0x56789ABCU;
    auto packed = pack(0x12345678ULL, pack_bytes<4>(original));
    auto [extracted, remaining] = unpack<std::uint32_t, 4, std::uint64_t>(packed);

    REQUIRE(extracted == original);
    REQUIRE(remaining == 0x12345678ULL);
}

TEST_CASE("在 pack 最大 N 值时应该正确处理", "[coding]")
{
    // N=2 时，dest 左移 16 位，src<2> 掩码到 2 字节后 OR 上去
    // pack(0x1234U, pack_bytes<2>(0x5678U)) = (0x1234 << 16) | 0x5678 = 0x12345678
    auto result = pack(0x1234U, pack_bytes<2>(0x5678U));
    REQUIRE(result == 0x12345678U);  // dest 在高位，src 在低位
}

TEST_CASE("在 pack 时 dest 为 0 应该正确处理", "[coding]")
{
    auto result = pack(0ULL, pack_bytes<2>(0xABCDU));
    REQUIRE(result == 0xABCDULL);
}

TEST_CASE("在 pack 时 dest 为全 1 应该正确处理", "[coding]")
{
    // dest 为全 1，左移后 OR 上 src
    // ~0ULL 左移 8 位 = 0xFFFFFFFFFFFFFF00
    auto result = pack(~0ULL, pack_bytes<1>(0x34U));
    REQUIRE(result == 0xFFFFFFFFFFFFFF34ULL);
}

TEST_CASE("在 pack 时 src 为全 0 应该正确处理", "[coding]")
{
    auto result = pack(0x1234ULL, pack_bytes<2>(0U));
    REQUIRE(result == 0x12340000ULL);
}

TEST_CASE("在 pack 时 src 为全 1 应该正确处理", "[coding]")
{
    // src 为全 1，应该被掩码到 N 字节大小
    auto result = pack(0x12ULL, pack_bytes<1>(~0U));
    REQUIRE(result == 0x12FFULL);
}

TEST_CASE("在 unpack 时提取全 1 掩码应该正确处理", "[coding]")
{
    // N=2: 从 0x123456FFFF00ULL 的低 16 位（2 字节）提取 0xFF00，剩余 0x123456FF
    auto [extracted, remaining] = unpack<std::uint16_t, 2>(0x123456FFFF00ULL);
    REQUIRE(extracted == 0xFF00U);
    REQUIRE(remaining == 0x123456FFULL);
}

TEST_CASE("在 unpack 时提取全 0 应该正确处理", "[coding]")
{
    // N=2: 从 0x123456000000ULL 的低 16 位提取 0，剩余右移 16 位后是 0x123456
    const std::uint64_t input = 0x123456000000ULL;
    auto [extracted, remaining] = unpack<std::uint16_t, 2>(input);
    REQUIRE(extracted == 0U);
    // remaining 应该是 input >> 16
    // 0x123456000000 >> 16 = 0x123456
    REQUIRE(remaining == (input >> 16));
}

TEST_CASE("在多次 pack 后再多次 unpack 应该恢复原始值", "[coding]")
{
    // 拼接 0x12 0x34 0x56，然后逆向解包
    auto step1 = pack(0ULL, pack_bytes<1>(0x12U));
    auto step2 = pack(step1, pack_bytes<1>(0x34U));
    auto step3 = pack(step2, pack_bytes<1>(0x56U));

    auto [e1, r1] = unpack<std::uint8_t, 1>(step3);
    auto [e2, r2] = unpack<std::uint8_t, 1>(r1);
    auto [e3, r3] = unpack<std::uint8_t, 1>(r2);

    REQUIRE(e1 == 0x56U);
    REQUIRE(e2 == 0x34U);
    REQUIRE(e3 == 0x12U);
    REQUIRE(r3 == 0ULL);
}

TEST_CASE("pack 应该能够正确编码 32 位地址的段偏移（模拟段式内存寻址）", "[coding]")
{
    // 模拟场景：编码 16 位段地址 + 16 位偏移地址
    // 段地址：0x1000，偏移：0x5678
    const std::uint32_t segment = 0x1000U;
    const std::uint32_t offset = 0x5678U;

    auto combined = pack(segment, pack_bytes<2>(offset));
    REQUIRE(combined == 0x10005678UL);

    auto [extracted_offset, extracted_segment] = unpack<std::uint16_t, 2>(combined);
    REQUIRE(extracted_offset == offset);
    REQUIRE(extracted_segment == segment);
}

TEST_CASE("pack 应该能够编码多字节数据结构（模拟网络数据包头）", "[coding]")
{
    // 模拟网络包头：版本(1字节) + 类型(1字节) + 长度(2字节)
    // 使用 pack 逐步构建
    const std::uint8_t version = 0x01U;
    const std::uint8_t type = 0x02U;
    const std::uint16_t length = 0x0100U;  // 256 字节

    auto step1 = pack(0ULL, pack_bytes<1>(version));
    auto step2 = pack(step1, pack_bytes<1>(type));
    auto step3 = pack(step2, pack_bytes<2>(length));

    // pack 的逻辑：dest 左移 N 字节，然后 OR src
    // step1: pack(0, 0x01) = 0x01
    // step2: pack(0x01, 0x02) = (0x01 << 8) | 0x02 = 0x0102
    // step3: pack(0x0102, 0x0100) = (0x0102 << 16) | 0x0100 = 0x01020100
    REQUIRE(step3 == 0x01020100ULL);
}

TEST_CASE("unpack 应该能够正确解析多字节编码的数据", "[coding]")
{
    // 从网络包头中逐步解包
    // 对应 pack 中的 0x01020100，结构是：
    // 低 2 字节：0x0100（length）
    // 然后 1 字节：0x02（type）
    // 最后 1 字节：0x01（version）
    const std::uint64_t packet_header = 0x01020100ULL;

    auto [len, temp1] = unpack<std::uint16_t, 2>(packet_header);
    REQUIRE(len == 0x0100U);  // 长度字段

    auto [type_byte, temp2] = unpack<std::uint8_t, 1>(temp1);
    REQUIRE(type_byte == 0x02U);  // 类型字段

    auto [version_byte, temp3] = unpack<std::uint8_t, 1>(temp2);
    REQUIRE(version_byte == 0x01U);  // 版本字段

    REQUIRE(temp3 == 0ULL);  // 没有剩余数据
}

TEST_CASE("pack 应该能够处理大值 pack_bytes 到小类型", "[coding]")
{
    // 测试掩码功能：大值被正确掩码到 N 字节大小
    const std::uint32_t large_value = 0xDEADBEEFU;

    // 只 pack 最后 3 字节
    auto result = pack(0x12ULL, pack_bytes<3>(large_value));
    // 掩码后应该取低 3 字节：0xADBEEF
    REQUIRE(result == (0x12ULL << 24 | 0xADBEEFUL));
}

TEST_CASE("unpack 应该能够多次连续解包还原完整值", "[coding]")
{
    // 将 64 位值分解为 4 个 16 位值
    const std::uint64_t original = 0x0123456789ABCDEFULL;

    auto [part1, rem1] = unpack<std::uint16_t, 2>(original);
    REQUIRE(part1 == 0xCDEFU);

    auto [part2, rem2] = unpack<std::uint16_t, 2>(rem1);
    REQUIRE(part2 == 0x89ABU);

    auto [part3, rem3] = unpack<std::uint16_t, 2>(rem2);
    REQUIRE(part3 == 0x4567U);

    auto [part4, rem4] = unpack<std::uint16_t, 2>(rem3);
    REQUIRE(part4 == 0x0123U);

    REQUIRE(rem4 == 0ULL);
}

TEST_CASE("pack 和 unpack 应该能够正确处理最大 N 值 (N = sizeof(T) - 1)", "[coding]")
{
    // 对于 uint32_t，最大 N=3（4-1）
    const std::uint32_t dest = 0x01020304U;
    const std::uint32_t src = 0xFFFFFFFFU;

    // pack 3 字节到 uint32_t
    auto result = pack(dest, pack_bytes<3>(src));
    // 应该左移 24 位，然后 OR 上 src（但 src 掩码到 3 字节）
    REQUIRE(result == ((dest << 24) | 0xFFFFFFU));
}

TEST_CASE("pack 应该能够处理交替的不同大小字节编码", "[coding]")
{
    // 混合不同大小的编码：1 字节 + 2 字节 + 1 字节 + 2 字节
    auto step1 = pack(0UL, pack_bytes<1>(0xAAU));      // 0x000000AA
    auto step2 = pack(step1, pack_bytes<2>(0xBBCCU));   // 0x0000AABBCC
    auto step3 = pack(step2, pack_bytes<1>(0xDDU));     // 0x00AABBCCDD
    auto step4 = pack(step3, pack_bytes<2>(0xEEFFU));   // 0xAABBCCDDEEFF

    // 逆向验证
    auto [p1, r1] = unpack<std::uint16_t, 2>(step4);
    REQUIRE(p1 == 0xEEFFU);

    auto [p2, r2] = unpack<std::uint8_t, 1>(r1);
    REQUIRE(p2 == 0xDDU);

    auto [p3, r3] = unpack<std::uint16_t, 2>(r2);
    REQUIRE(p3 == 0xBBCCU);

    auto [p4, r4] = unpack<std::uint8_t, 1>(r3);
    REQUIRE(p4 == 0xAAU);

    REQUIRE(r4 == 0ULL);
}

// ── varint::length() 函数 ────────────────────────────────────────────────────

TEST_CASE("varint::length() 对于单字节值应该返回 0", "[coding]")
{
    REQUIRE(varint::length(uint32_t{0}) == 0);
    REQUIRE(varint::length(uint32_t{1}) == 0);
    REQUIRE(varint::length(uint32_t{127}) == 0);
}

TEST_CASE("varint::length() 对于两字节值应该返回 1", "[coding]")
{
    REQUIRE(varint::length(uint32_t{128}) == 1);
    REQUIRE(varint::length(uint32_t{16383}) == 1);
}

TEST_CASE("varint::length() 对于三字节值应该返回 2", "[coding]")
{
    REQUIRE(varint::length(uint32_t{16384}) == 2);
    REQUIRE(varint::length(uint32_t{2097151}) == 2);
}

TEST_CASE("varint::length() 对于四字节值应该返回 3", "[coding]")
{
    REQUIRE(varint::length(uint32_t{2097152}) == 3);
    REQUIRE(varint::length(uint32_t{268435455}) == 3);
}

TEST_CASE("varint::length() 对于五字节值应该返回 4", "[coding]")
{
    REQUIRE(varint::length(uint32_t{268435456}) == 4);
    REQUIRE(varint::length(std::numeric_limits<uint32_t>::max()) == 4);
}

TEST_CASE("varint::length() 对于 uint64 边界值应该返回正确值", "[coding]")
{
    REQUIRE(varint::length(uint64_t{0}) == 0);
    REQUIRE(varint::length(uint64_t{127}) == 0);
    REQUIRE(varint::length(uint64_t{128}) == 1);
    REQUIRE(varint::length(uint64_t{16383}) == 1);
    REQUIRE(varint::length(uint64_t{16384}) == 2);
    REQUIRE(varint::length(std::numeric_limits<uint64_t>::max()) == 9);
}

TEST_CASE("varint::length() 与实际编码字节数应该一致", "[coding]")
{
    const std::vector<uint32_t> values{0, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, std::numeric_limits<uint32_t>::max()};

    for (auto val : values) {
        std::vector<std::byte> buffer;
        varint::encode(std::back_inserter(buffer), val);

        // length() 返回的是额外字节数（不包括第一个字节）
        // 总字节数 = length() + 1
        REQUIRE(varint::length(val) + 1 == buffer.size());
    }
}

// ── fixed 编码与解码 ──────────────────────────────────────────────────────

TEST_CASE("fixed::encode() 应该输出固定字节数", "[coding]")
{
    std::vector<std::byte> buf1;
    fixed::encode(std::back_inserter(buf1), uint8_t{42});
    REQUIRE(buf1.size() == 1);

    std::vector<std::byte> buf2;
    fixed::encode(std::back_inserter(buf2), uint32_t{42});
    REQUIRE(buf2.size() == 4);

    std::vector<std::byte> buf3;
    fixed::encode(std::back_inserter(buf3), uint64_t{42});
    REQUIRE(buf3.size() == 8);
}

TEST_CASE("fixed::encode() 应该使用小端字节顺序", "[coding]")
{
    std::vector<std::byte> buffer;
    fixed::encode(std::back_inserter(buffer), uint32_t{0x12345678});

    REQUIRE(buffer.size() == 4);
    REQUIRE(buffer[0] == std::byte{0x78});  // 最低字节
    REQUIRE(buffer[1] == std::byte{0x56});
    REQUIRE(buffer[2] == std::byte{0x34});
    REQUIRE(buffer[3] == std::byte{0x12});  // 最高字节
}

TEST_CASE("fixed::encode() 对零值应该输出零字节", "[coding]")
{
    std::vector<std::byte> buffer;
    fixed::encode(std::back_inserter(buffer), uint32_t{0});

    REQUIRE(buffer.size() == 4);
    REQUIRE(buffer[0] == std::byte{0});
    REQUIRE(buffer[1] == std::byte{0});
    REQUIRE(buffer[2] == std::byte{0});
    REQUIRE(buffer[3] == std::byte{0});
}

TEST_CASE("fixed::encode() 对最大值应该输出全 1 字节", "[coding]")
{
    std::vector<std::byte> buffer;
    fixed::encode(std::back_inserter(buffer), std::numeric_limits<uint16_t>::max());

    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer[0] == std::byte{0xFF});
    REQUIRE(buffer[1] == std::byte{0xFF});
}

TEST_CASE("fixed::decode() 应该恢复编码后的值", "[coding]")
{
    std::vector<std::byte> buffer;
    const uint32_t original = 0x12345678;
    fixed::encode(std::back_inserter(buffer), original);

    auto [decoded, _] = fixed::decode<uint32_t>(buffer.begin());
    REQUIRE(decoded == original);
}

TEST_CASE("fixed::encode/decode() 应该往返保持值一致", "[coding]")
{
    const std::vector<uint32_t> values{0, 1, 128, 255, 256, 16383, 16384, 1000000, std::numeric_limits<uint32_t>::max()};

    for (auto val : values) {
        std::vector<std::byte> buffer;
        fixed::encode(std::back_inserter(buffer), val);
        auto [decoded, _] = fixed::decode<uint32_t>(buffer.begin());

        REQUIRE(decoded == val);
    }
}

TEST_CASE("fixed::encode() 到 char 迭代器应该正确编码", "[coding]")
{
    std::string buffer;
    const uint32_t value = 0xDEADBEEF;
    fixed::encode(std::back_inserter(buffer), value);

    REQUIRE(buffer.size() == 4);
    REQUIRE(static_cast<unsigned char>(buffer[0]) == 0xEF);
    REQUIRE(static_cast<unsigned char>(buffer[1]) == 0xBE);
    REQUIRE(static_cast<unsigned char>(buffer[2]) == 0xAD);
    REQUIRE(static_cast<unsigned char>(buffer[3]) == 0xDE);
}

TEST_CASE("fixed::decode() 从 char 迭代器应该正确解码", "[coding]")
{
    std::string buffer;
    const uint32_t original = 0x12345678;
    fixed::encode(std::back_inserter(buffer), original);

    auto [decoded, _] = fixed::decode<uint32_t>(buffer.c_str());
    REQUIRE(decoded == original);
}

TEST_CASE("fixed::encode() 连续编码多个值应该按顺序写入", "[coding]")
{
    std::vector<std::byte> buffer;
    fixed::encode(std::back_inserter(buffer), uint32_t{0x11223344});
    fixed::encode(std::back_inserter(buffer), uint16_t{0x5566});

    REQUIRE(buffer.size() == 6);
    REQUIRE(buffer[0] == std::byte{0x44});
    REQUIRE(buffer[1] == std::byte{0x33});
    REQUIRE(buffer[2] == std::byte{0x22});
    REQUIRE(buffer[3] == std::byte{0x11});
    REQUIRE(buffer[4] == std::byte{0x66});
    REQUIRE(buffer[5] == std::byte{0x55});
}

TEST_CASE("fixed::decode() 连续解码多个值应该按顺序恢复", "[coding]")
{
    std::vector<std::byte> buffer;
    const uint32_t val1 = 0x11223344;
    const uint16_t val2 = 0x5566;
    
    fixed::encode(std::back_inserter(buffer), val1);
    fixed::encode(std::back_inserter(buffer), val2);

    auto [dec1, iter1] = fixed::decode<uint32_t>(buffer.begin());
    REQUIRE(dec1 == val1);

    auto [dec2, iter2] = fixed::decode<uint16_t>(iter1);
    REQUIRE(dec2 == val2);

    REQUIRE(iter2 == buffer.end());
}

TEST_CASE("fixed::encode() 与 varint::encode() 编码大小不同", "[coding]")
{
    // fixed 总是编码完整字节数，varint 用变长编码
    std::vector<std::byte> fixed_buf;
    std::vector<std::byte> varint_buf;

    const uint32_t small_value = 42;
    fixed::encode(std::back_inserter(fixed_buf), small_value);
    varint::encode(std::back_inserter(varint_buf), small_value);

    // 对于小值，fixed 使用 4 字节，varint 使用 1 字节
    REQUIRE(fixed_buf.size() == 4);
    REQUIRE(varint_buf.size() == 1);
}

TEST_CASE("fixed::encode() 对大值比 varint 更高效", "[coding]")
{
    std::vector<std::byte> fixed_buf;
    std::vector<std::byte> varint_buf;

    const uint32_t large_value = std::numeric_limits<uint32_t>::max();
    fixed::encode(std::back_inserter(fixed_buf), large_value);
    varint::encode(std::back_inserter(varint_buf), large_value);

    // 对于大值，fixed 使用 4 字节，varint 使用 5 字节
    REQUIRE(fixed_buf.size() == 4);
    REQUIRE(varint_buf.size() == 5);
}
