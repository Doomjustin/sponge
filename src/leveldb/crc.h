#ifndef SPONGE_LEVELDB_CRC_H
#define SPONGE_LEVELDB_CRC_H

#include <cstdint>
#include <string_view>

namespace spg::leveldb {

struct crc {
    crc() = delete;
        
    struct MaskedT {
        uint32_t value;
    };

    struct AddMaskT {};

    static constexpr AddMaskT add_mask{};

    static constexpr auto masked(uint32_t value) noexcept -> MaskedT
    {
        return { value };
    }

    static auto compute(uint8_t type, std::string_view data) -> uint32_t;

    static auto compute(uint8_t type, std::string_view data, [[maybe_unused]] AddMaskT) -> uint32_t;

    static auto verify(uint32_t expected, uint8_t type, std::string_view data) -> bool;

    static auto verify(MaskedT expected, uint8_t type, std::string_view data) -> bool;
};

} // namespace spg::leveldb

#endif // SPONGE_LEVELDB_CRC_H
