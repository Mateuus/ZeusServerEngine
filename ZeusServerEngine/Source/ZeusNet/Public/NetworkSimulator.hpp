#pragma once

#include <cstdint>

namespace Zeus::Net
{
/** Perda UDP simulada na receção (permille 0–1000). Thread única. */
class NetworkSimulator
{
public:
    void Configure(std::uint16_t dropPermille, std::uint32_t salt);

    [[nodiscard]] bool ShouldDropInbound();

private:
    std::uint16_t dropPermille_ = 0;
    std::uint32_t salt_ = 1;
    std::uint32_t rngState_ = 0xC0FFEEu;
};
} // namespace Zeus::Net
