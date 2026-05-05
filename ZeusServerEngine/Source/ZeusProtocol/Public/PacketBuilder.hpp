#pragma once

#include "PacketConstants.hpp"
#include "PacketHeader.hpp"
#include "PacketWriter.hpp"
#include "ZeusResult.hpp"

namespace Zeus::Protocol
{
/** Reserva cabeçalho e escreve payload; `Finalize` preenche wire + `payloadSize`. */
class PacketBuilder
{
public:
    /** Prepara buffer com cursor após o cabeçalho wire. */
    void Reset();

    [[nodiscard]] PacketWriter& PayloadWriter() { return writer_; }

    /** Escreve cabeçalho no início; `header.payloadSize` é recalculado a partir do tamanho atual. */
    [[nodiscard]] ZeusResult Finalize(PacketHeader header);

    [[nodiscard]] const std::uint8_t* Buffer() const { return writer_.Data(); }
    [[nodiscard]] std::size_t ByteSize() const { return writer_.Size(); }

private:
    PacketWriter writer_;
};
} // namespace Zeus::Protocol
