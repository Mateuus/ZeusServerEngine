#pragma once

#include "ZeusNetwork.h"

namespace ZeusNetWire
{
	static constexpr uint8 ZeusMagicBytes[4] = { 'Z', 'E', 'U', 'S' };

	/** uint32 LE: primeiro byte no wire = 'Z' (NETWORK.md / ClientTest). */
	inline uint32 ZeusMagicUInt32()
	{
		return static_cast<uint32>(ZeusMagicBytes[0])
			| (static_cast<uint32>(ZeusMagicBytes[1]) << 8)
			| (static_cast<uint32>(ZeusMagicBytes[2]) << 16)
			| (static_cast<uint32>(ZeusMagicBytes[3]) << 24);
	}

	static constexpr uint16 ZeusProtocolVersion = 2;
	static constexpr int32 ZeusWireHeaderBytes = 32;
	static constexpr int32 ZeusMaxPacketBytes = 1200;
}

struct ZEUSNETWORK_API FZeusPacketHeader
{
	uint32 Magic = 0;
	uint16 Version = 0;
	uint16 HeaderSize = 0;

	uint16 Opcode = 0;
	uint8 Channel = 0;
	uint8 Delivery = 0;

	uint32 Sequence = 0;
	uint32 Ack = 0;
	uint32 AckBits = 0;

	uint32 ConnectionId = 0;
	uint16 PayloadSize = 0;
	uint16 Flags = 0;

	/** Escreve exatamente 32 bytes LE (não usar sizeof). */
	void WriteBytes(uint8* Dest) const;

	/** Lê 32 bytes LE; valida magic, version, headerSize. */
	static bool TryRead(const uint8* Src, int32 SrcLen, FZeusPacketHeader& OutHeader);
};
