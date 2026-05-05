#include "ZeusPacketReader.h"

#include "Containers/StringConv.h"

FZeusPacketReader::FZeusPacketReader(const uint8* InData, const int32 InPayloadLen)
	: Data(InData)
	, PayloadLen(InPayloadLen)
{
}

bool FZeusPacketReader::ReadUInt8(uint8& OutV)
{
	if (Cursor + 1 > PayloadLen)
	{
		return false;
	}
	OutV = Data[Cursor++];
	return true;
}

bool FZeusPacketReader::ReadUInt16(uint16& OutV)
{
	if (Cursor + 2 > PayloadLen)
	{
		return false;
	}
	OutV = static_cast<uint16>(Data[Cursor]) | (static_cast<uint16>(Data[Cursor + 1]) << 8);
	Cursor += 2;
	return true;
}

bool FZeusPacketReader::ReadUInt32(uint32& OutV)
{
	if (Cursor + 4 > PayloadLen)
	{
		return false;
	}
	OutV = static_cast<uint32>(Data[Cursor])
		| (static_cast<uint32>(Data[Cursor + 1]) << 8)
		| (static_cast<uint32>(Data[Cursor + 2]) << 16)
		| (static_cast<uint32>(Data[Cursor + 3]) << 24);
	Cursor += 4;
	return true;
}

bool FZeusPacketReader::ReadUInt64(uint64& OutV)
{
	uint32 Lo = 0;
	uint32 Hi = 0;
	if (!ReadUInt32(Lo) || !ReadUInt32(Hi))
	{
		return false;
	}
	OutV = (static_cast<uint64>(Hi) << 32) | static_cast<uint64>(Lo);
	return true;
}

bool FZeusPacketReader::ReadBytes(void* Dest, const int32 Len)
{
	if (Len < 0 || Cursor + Len > PayloadLen)
	{
		return false;
	}
	FMemory::Memcpy(Dest, Data + Cursor, Len);
	Cursor += Len;
	return true;
}

bool FZeusPacketReader::ReadStringUtf8(FString& OutS)
{
	uint16 LenU16 = 0;
	if (!ReadUInt16(LenU16))
	{
		return false;
	}
	const int32 Len = LenU16;
	if (Cursor + Len > PayloadLen)
	{
		return false;
	}
	if (Len == 0)
	{
		OutS.Empty();
		return true;
	}
	const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Data + Cursor), Len);
	OutS = FString(static_cast<int32>(Conv.Length()), Conv.Get());
	Cursor += Len;
	return true;
}
