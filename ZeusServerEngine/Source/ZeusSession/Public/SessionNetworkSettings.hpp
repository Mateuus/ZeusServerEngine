#pragma once

#include <cstdint>

namespace Zeus::Session
{
/** Políticas de rede carregadas de `Config/server.json` (Parte 2.5). */
struct SessionNetworkSettings
{
    int connectionTimeoutMs = 30000;
    bool networkDebugAck = false;

    double reliableResendIntervalSec = 0.25;
    std::uint32_t reliableMaxResends = 12;

    std::uint32_t maxLoadingFragmentCount = 4096;
    std::uint32_t maxReassemblyBytes = 4u * 1024u * 1024u;
    int reassemblyTimeoutMs = 60000;

    std::uint32_t maxOrderedPendingPerChannel = 64;
    std::uint32_t maxOrderedGap = 128;
};
} // namespace Zeus::Session
