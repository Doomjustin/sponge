#include "crc.h"

#include <crc32c/crc32c.h>

namespace spg::leveldb {
namespace {

// 掩码常量：(crc >> 15) | (crc << 17) 加上这个魔数
constexpr uint32_t MASK_DELTA = 0xa282ead8ul;

[[nodiscard]]
constexpr auto mask(uint32_t crc) noexcept -> uint32_t {
    // 将 CRC 循环右移 15 位，然后加上魔数
    return ((crc >> 15) | (crc << 17)) + MASK_DELTA;
}

[[nodiscard]]
constexpr auto unmask(uint32_t masked_crc) noexcept -> uint32_t {
    auto rot = masked_crc - MASK_DELTA;
    return ((rot >> 17) | (rot << 15));
}

} // namespace

auto crc::compute(uint8_t type, std::string_view data) -> uint32_t
{
    uint32_t result = 0;
    result = crc32c::Extend(result, &type, 1);
    result = crc32c::Extend(result, reinterpret_cast<const uint8_t*>(data.data()), data.size());
    return result;
}

auto crc::compute(uint8_t type, std::string_view data, [[maybe_unused]] AddMaskT) -> uint32_t
{
    uint32_t result = 0;
    result = crc32c::Extend(result, &type, 1);
    result = crc32c::Extend(result, reinterpret_cast<const uint8_t*>(data.data()), data.size());
    return mask(result);
}

auto crc::verity(uint32_t expected, uint8_t type, std::string_view data) -> bool
{
    uint32_t actual = 0;
    actual = crc32c::Extend(actual, &type, 1);
    actual = crc32c::Extend(actual, reinterpret_cast<const uint8_t*>(data.data()), data.size());
    return expected == actual;
}

auto crc::verity(MaskedT expected, uint8_t type, std::string_view data) -> bool
{
    return verity(unmask(expected.value), type, data);
}

} // namespace spg::leveldb
