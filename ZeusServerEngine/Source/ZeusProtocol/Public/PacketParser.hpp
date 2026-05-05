#pragma once

#include "PacketConstants.hpp"
#include "PacketHeader.hpp"
#include "ZeusResult.hpp"

#include <cstddef>
#include <cstdint>

namespace Zeus::Protocol
{
class PacketParser
{
public:
    struct Output
    {
        PacketHeader header{};
        const std::uint8_t* payload = nullptr;
        std::size_t payloadSize = 0;
    };

    /**
     * Valida magic, versão, tamanhos e devolve vista do payload.
     * @param strictOpcodes se true, rejeita opcode não listado em `IsKnownOpcode`.
     */
    static ZeusResult Parse(const std::uint8_t* data, std::size_t length, bool strictOpcodes, Output& out);

    static bool IsKnownOpcode(std::uint16_t opcode);
};
} // namespace Zeus::Protocol
