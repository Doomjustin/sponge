#include "coding.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace spg::leveldb;

TEST_CASE("fixed write/read 在 subspan 上应正常工作", "[leveldb][coding]")
{
    std::vector<std::byte> buffer(16, std::byte{ 0 });

    fixed::write<uint32_t>(std::span<std::byte>{ buffer }.subspan(8, sizeof(uint32_t)),
                           0x01020304U);

    auto value = fixed::read<uint32_t>(std::span<const std::byte>{ buffer }.subspan(8, sizeof(uint32_t)));
    REQUIRE(value == 0x01020304U);
}

TEST_CASE("fixed append 应向 vector 追加字节", "[leveldb][coding]")
{
    std::vector<std::byte> buffer;

    fixed::append<uint16_t>(buffer, 0xABCDU);
    fixed::append<uint16_t>(buffer, 0x0012U);

    REQUIRE(buffer.size() == sizeof(uint16_t) * 2);

    auto first = fixed::read<uint16_t>(std::span<const std::byte>{ buffer }.subspan(0, sizeof(uint16_t)));
    auto second = fixed::read<uint16_t>(std::span<const std::byte>{ buffer }.subspan(sizeof(uint16_t), sizeof(uint16_t)));

    REQUIRE(first == 0xABCDU);
    REQUIRE(second == 0x0012U);
}

TEST_CASE("fixed append 应向 vector 追加 string_view 字节", "[leveldb][coding]")
{
    std::vector<std::byte> buffer;

    fixed::append(buffer, std::string_view{ "key" });

    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer[0] == std::byte{ 'k' });
    REQUIRE(buffer[1] == std::byte{ 'e' });
    REQUIRE(buffer[2] == std::byte{ 'y' });
}

TEST_CASE("fixed append 应向 string 追加字节", "[leveldb][coding]")
{
    std::string buffer;

    fixed::append<uint32_t>(buffer, 0xDEADBEEFU);
    fixed::append<uint32_t>(buffer, 0x00000001U);

    REQUIRE(buffer.size() == sizeof(uint32_t) * 2);

    const auto* bytes = reinterpret_cast<const std::byte*>(buffer.data());
    std::span<const std::byte> view{ bytes, buffer.size() };

    auto first = fixed::read<uint32_t>(view.subspan(0, sizeof(uint32_t)));
    auto second = fixed::read<uint32_t>(view.subspan(sizeof(uint32_t), sizeof(uint32_t)));

    REQUIRE(first == 0xDEADBEEFU);
    REQUIRE(second == 0x00000001U);
}

TEST_CASE("varint append 应编码为预期字节序列", "[leveldb][coding]")
{
    std::vector<std::byte> buffer;

    varint::append<uint32_t>(buffer, 0U);
    varint::append<uint32_t>(buffer, 127U);
    varint::append<uint32_t>(buffer, 128U);
    varint::append<uint32_t>(buffer, 300U);

    const std::array<std::byte, 6> expected{
        std::byte{ 0x00 }, std::byte{ 0x7F }, std::byte{ 0x80 },
        std::byte{ 0x01 }, std::byte{ 0xAC }, std::byte{ 0x02 },
    };

    REQUIRE(buffer.size() == expected.size());
    REQUIRE(buffer == std::vector<std::byte>{ expected.begin(), expected.end() });
}

TEST_CASE("varint append 应为 string 编码预期字节序列", "[leveldb][coding]")
{
    std::string buffer;

    varint::append<uint32_t>(buffer, 300U);

    REQUIRE(buffer.size() == 2);
    REQUIRE(static_cast<unsigned char>(buffer[0]) == 0xACU);
    REQUIRE(static_cast<unsigned char>(buffer[1]) == 0x02U);
}

TEST_CASE("varint read 应解码预期值并返回消耗字节数", "[leveldb][coding]")
{
    std::vector<std::byte> buffer;
    varint::append<uint32_t>(buffer, 300U);
    buffer.push_back(std::byte{ 0xFF });

    auto decoded = varint::read<uint32_t>(std::span<const std::byte>{ buffer });

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->first == 300U);
    REQUIRE(decoded->second == 2);
}

TEST_CASE("varint read 从 string 读取时应解码预期值", "[leveldb][coding]")
{
    std::string buffer;
    varint::append<uint32_t>(buffer, 300U);

    auto decoded = varint::read<uint32_t>(std::string_view{ buffer });

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->first == 300U);
    REQUIRE(decoded->second == 2);
}

TEST_CASE("varint read 在截断输入时应返回 nullopt", "[leveldb][coding]")
{
    std::vector<std::byte> buffer{ std::byte{ 0x80 } };

    auto decoded = varint::read<uint32_t>(std::span<const std::byte>{ buffer });

    REQUIRE_FALSE(decoded.has_value());
}

TEST_CASE("varint read 在溢出输入时应返回 nullopt", "[leveldb][coding]")
{
    std::vector<std::byte> buffer{
        std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF },
        std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0x01 },
    };

    auto decoded = varint::read<uint32_t>(std::span<const std::byte>{ buffer });

    REQUIRE_FALSE(decoded.has_value());
}
