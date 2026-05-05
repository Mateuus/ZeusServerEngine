#pragma once

#include <cstdint>

namespace Zeus::Net
{
/** RTT estimado a partir de ping/pong (ms). */
struct NetworkDiagnostics
{
    std::uint64_t LastRttMs = 0;
    std::uint64_t LastClientTimeMs = 0;
    std::uint64_t LastServerTimeMs = 0;

    void OnPongSample(std::uint64_t clientTimeMs, std::uint64_t serverTimeMs, std::uint64_t wallNowMs);
};
} // namespace Zeus::Net
