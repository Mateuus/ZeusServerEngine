#pragma once

#include <cstdint>

namespace Zeus::Net
{
/** Contadores por tick (thread única do servidor). */
struct PacketStats
{
    std::uint64_t DatagramsReceived = 0;
    std::uint64_t DatagramsRejectedParse = 0;
    std::uint64_t DatagramsRejectedOpcodeRules = 0;
    std::uint64_t DatagramsSimDropped = 0;
    std::uint64_t ReliableResends = 0;
    std::uint64_t OutboundDatagrams = 0;
    std::uint64_t LoadingAssembliesCompleted = 0;
};
} // namespace Zeus::Net
