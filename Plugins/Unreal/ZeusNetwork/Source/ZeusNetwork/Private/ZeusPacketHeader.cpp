#include "ZeusPacketHeader.h"

namespace ZeusNetWireLocal
{
	static void WriteU16LE(uint8* D, uint16 V)
	{
		D[0] = static_cast<uint8>(V & 0xFFu);
		D[1] = static_cast<uint8>((V >> 8) & 0xFFu);
	}

	static void WriteU32LE(uint8* D, uint32 V)
	{
		D[0] = static_cast<uint8>(V & 0xFFu);
		D[1] = static_cast<uint8>((V >> 8) & 0xFFu);
		D[2] = static_cast<uint8>((V >> 16) & 0xFFu);
		D[3] = static_cast<uint8>((V >> 24) & 0xFFu);
	}

	static uint16 ReadU16LE(const uint8* D)
	{
		return static_cast<uint16>(D[0]) | (static_cast<uint16>(D[1]) << 8);
	}

	static uint32 ReadU32LE(const uint8* D)
	{
		return static_cast<uint32>(D[0])
			| (static_cast<uint32>(D[1]) << 8)
			| (static_cast<uint32>(D[2]) << 16)
			| (static_cast<uint32>(D[3]) << 24);
	}
}

void FZeusPacketHeader::WriteBytes(uint8* Dest) const
{
	using namespace ZeusNetWireLocal;
	WriteU32LE(Dest + 0, Magic);
	WriteU16LE(Dest + 4, Version);
	WriteU16LE(Dest + 6, HeaderSize);
	WriteU16LE(Dest + 8, Opcode);
	Dest[10] = Channel;
	Dest[11] = Delivery;
	WriteU32LE(Dest + 12, Sequence);
	WriteU32LE(Dest + 16, Ack);
	WriteU32LE(Dest + 20, AckBits);
	WriteU32LE(Dest + 24, ConnectionId);
	WriteU16LE(Dest + 28, PayloadSize);
	WriteU16LE(Dest + 30, Flags);
}

bool FZeusPacketHeader::TryRead(const uint8* Src, const int32 SrcLen, FZeusPacketHeader& OutHeader)
{
	if (SrcLen < ZeusNetWire::ZeusWireHeaderBytes)
	{
		return false;
	}

	using namespace ZeusNetWireLocal;
	OutHeader.Magic = ReadU32LE(Src + 0);
	OutHeader.Version = ReadU16LE(Src + 4);
	OutHeader.HeaderSize = ReadU16LE(Src + 6);
	OutHeader.Opcode = ReadU16LE(Src + 8);
	OutHeader.Channel = Src[10];
	OutHeader.Delivery = Src[11];
	OutHeader.Sequence = ReadU32LE(Src + 12);
	OutHeader.Ack = ReadU32LE(Src + 16);
	OutHeader.AckBits = ReadU32LE(Src + 20);
	OutHeader.ConnectionId = ReadU32LE(Src + 24);
	OutHeader.PayloadSize = ReadU16LE(Src + 28);
	OutHeader.Flags = ReadU16LE(Src + 30);

	if (OutHeader.Magic != ZeusNetWire::ZeusMagicUInt32())
	{
		return false;
	}
	if (OutHeader.Version != ZeusNetWire::ZeusProtocolVersion)
	{
		return false;
	}
	if (OutHeader.HeaderSize != ZeusNetWire::ZeusWireHeaderBytes)
	{
		return false;
	}
	const int32 Total = ZeusNetWire::ZeusWireHeaderBytes + static_cast<int32>(OutHeader.PayloadSize);
	if (Total > ZeusNetWire::ZeusMaxPacketBytes || Total < ZeusNetWire::ZeusWireHeaderBytes)
	{
		return false;
	}
	if (SrcLen < Total)
	{
		return false;
	}
	return true;
}
