#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rota {

// The packing of every stored display color: r | g << 8 | b << 16
constexpr std::uint32_t pack_rgb(std::uint32_t r, std::uint32_t g, std::uint32_t b) {
    return (r & 0xFFU) | ((g & 0xFFU) << 8U) | ((b & 0xFFU) << 16U);
}

// Writes a packed color as three RGB8 bytes
inline void write_rgb24(std::vector<std::uint8_t>& bytes, std::size_t byte_index, std::uint32_t packed) {
    bytes[byte_index] = static_cast<std::uint8_t>(packed & 0xFFU);
    bytes[byte_index + 1] = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU);
    bytes[byte_index + 2] = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU);
}

}
