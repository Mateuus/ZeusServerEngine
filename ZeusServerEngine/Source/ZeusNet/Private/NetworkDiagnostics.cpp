#include "NetworkDiagnostics.hpp"

namespace Zeus::Net
{
void NetworkDiagnostics::OnPongSample(
    const std::uint64_t clientTimeMs,
    const std::uint64_t serverTimeMs,
    const std::uint64_t wallNowMs)
{
    LastClientTimeMs = clientTimeMs;
    LastServerTimeMs = serverTimeMs;
    if (wallNowMs > clientTimeMs)
    {
        LastRttMs = wallNowMs - clientTimeMs;
    }
    else
    {
        LastRttMs = 0;
    }
}
} // namespace Zeus::Net
