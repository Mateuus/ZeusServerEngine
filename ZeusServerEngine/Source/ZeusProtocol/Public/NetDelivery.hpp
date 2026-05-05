#pragma once

#include <cstdint>

namespace Zeus::Protocol
{
enum class ENetDelivery : std::uint8_t
{
    Unreliable = 0, //Não confiável (UDP)
    UnreliableSequenced = 1, //Não confiável sequenciado (UDP)
    Reliable = 2, //Confiavel (TCP)
    ReliableOrdered = 3, //Confiavel sequenciado (TCP)
};
} // namespace Zeus::Protocol
