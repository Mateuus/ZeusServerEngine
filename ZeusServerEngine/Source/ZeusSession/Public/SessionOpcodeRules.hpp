#pragma once

#include "NetChannel.hpp"
#include "NetDelivery.hpp"
#include "Opcodes.hpp"
#include "SessionState.hpp"

#include <cstdint>

namespace Zeus::Session
{
class ClientSession;

enum class OpcodeRuleResult : std::uint8_t
{
    Ok = 0,
    UnknownOpcode,
    WrongChannelOrDelivery,
    WrongSessionState
};

/** Matriz opcode → estado de sessão esperado + canal/entrega (referência GNS §6–§8). */
class SessionOpcodeRules
{
public:
    static OpcodeRuleResult ValidateClientOpcode(
        Zeus::Protocol::EOpcode opcode,
        Zeus::Protocol::ENetChannel channel,
        Zeus::Protocol::ENetDelivery delivery,
        bool hasNetConnection,
        ClientSession* sessionOrNull);
};
} // namespace Zeus::Session
