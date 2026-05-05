#include "SessionOpcodeRules.hpp"

#include "ClientSession.hpp"

namespace Zeus::Session
{
namespace
{
[[nodiscard]] bool DeliveryMatchesControl(
    const bool allowReliableFamily,
    const bool allowUnreliableFamily,
    const Zeus::Protocol::ENetDelivery d)
{
    if (allowUnreliableFamily &&
        (d == Zeus::Protocol::ENetDelivery::Unreliable || d == Zeus::Protocol::ENetDelivery::UnreliableSequenced))
    {
        return true;
    }
    if (allowReliableFamily &&
        (d == Zeus::Protocol::ENetDelivery::Reliable || d == Zeus::Protocol::ENetDelivery::ReliableOrdered))
    {
        return true;
    }
    return false;
}
} // namespace

OpcodeRuleResult SessionOpcodeRules::ValidateClientOpcode(
    const Zeus::Protocol::EOpcode opcode,
    const Zeus::Protocol::ENetChannel channel,
    const Zeus::Protocol::ENetDelivery delivery,
    const bool hasNetConnection,
    ClientSession* sessionOrNull)
{
    switch (opcode)
    {
    case Zeus::Protocol::EOpcode::C_CONNECT_REQUEST:
        if (channel != Zeus::Protocol::ENetChannel::Loading)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (!DeliveryMatchesControl(true, false, delivery))
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    case Zeus::Protocol::EOpcode::C_CONNECT_RESPONSE:
        if (!hasNetConnection || sessionOrNull == nullptr || sessionOrNull->GetState() != SessionState::Connecting)
        {
            return OpcodeRuleResult::WrongSessionState;
        }
        if (channel != Zeus::Protocol::ENetChannel::Loading)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (delivery != Zeus::Protocol::ENetDelivery::ReliableOrdered)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    case Zeus::Protocol::EOpcode::C_PING:
        if (!hasNetConnection || sessionOrNull == nullptr || sessionOrNull->GetState() != SessionState::Connected)
        {
            return OpcodeRuleResult::WrongSessionState;
        }
        if (channel != Zeus::Protocol::ENetChannel::Gameplay)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (!DeliveryMatchesControl(false, true, delivery))
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    case Zeus::Protocol::EOpcode::C_DISCONNECT:
        if (!hasNetConnection || sessionOrNull == nullptr || sessionOrNull->GetState() != SessionState::Connected)
        {
            return OpcodeRuleResult::WrongSessionState;
        }
        if (channel != Zeus::Protocol::ENetChannel::Gameplay)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (!DeliveryMatchesControl(true, false, delivery))
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    case Zeus::Protocol::EOpcode::C_TEST_RELIABLE:
        if (!hasNetConnection || sessionOrNull == nullptr || sessionOrNull->GetState() != SessionState::Connected)
        {
            return OpcodeRuleResult::WrongSessionState;
        }
        if (channel != Zeus::Protocol::ENetChannel::Gameplay)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (delivery != Zeus::Protocol::ENetDelivery::Reliable)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    case Zeus::Protocol::EOpcode::C_TEST_ORDERED:
        if (!hasNetConnection || sessionOrNull == nullptr || sessionOrNull->GetState() != SessionState::Connected)
        {
            return OpcodeRuleResult::WrongSessionState;
        }
        if (channel != Zeus::Protocol::ENetChannel::Gameplay)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (delivery != Zeus::Protocol::ENetDelivery::ReliableOrdered)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    case Zeus::Protocol::EOpcode::C_LOADING_FRAGMENT:
        if (!hasNetConnection || sessionOrNull == nullptr || sessionOrNull->GetState() != SessionState::Connected)
        {
            return OpcodeRuleResult::WrongSessionState;
        }
        if (channel != Zeus::Protocol::ENetChannel::Loading)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        if (delivery != Zeus::Protocol::ENetDelivery::ReliableOrdered)
        {
            return OpcodeRuleResult::WrongChannelOrDelivery;
        }
        return OpcodeRuleResult::Ok;

    default:
        return OpcodeRuleResult::UnknownOpcode;
    }
}
} // namespace Zeus::Session
