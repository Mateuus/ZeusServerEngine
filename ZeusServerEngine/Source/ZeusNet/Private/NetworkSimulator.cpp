#include "NetworkSimulator.hpp"

namespace Zeus::Net
{
void NetworkSimulator::Configure(const std::uint16_t dropPermille, const std::uint32_t salt)
{
    dropPermille_ = (dropPermille > 1000) ? 1000 : dropPermille;
    salt_ = salt == 0 ? 1u : salt;
    rngState_ ^= salt_;
}

bool NetworkSimulator::ShouldDropInbound()
{
    if (dropPermille_ == 0)
    {
        return false;
    }
    rngState_ = rngState_ * 1664525u + 1013904223u;
    const std::uint32_t v = rngState_ % 1000u;
    return v < static_cast<std::uint32_t>(dropPermille_);
}
} // namespace Zeus::Net
