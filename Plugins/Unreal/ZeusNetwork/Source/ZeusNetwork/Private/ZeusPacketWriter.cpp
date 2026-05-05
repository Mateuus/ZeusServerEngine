#include "ZeusPacketWriter.h"

#include "Containers/StringConv.h"

void FZeusPacketWriter::Reset()
{
	Buffer.Reset();
	Cursor = 0;
}

void FZeusPacketWriter::Clear()
{
	Buffer.Reset();
	Cursor = 0;
}

void FZeusPacketWriter::SetSize(const int32 NewSize)
{
	Buffer.SetNum(NewSize, EAllowShrinking::No);
}

void FZeusPacketWriter::WriteUInt8(const uint8 V)
{
	const int32 Need = Cursor + 1;
	if (Buffer.Num() < Need)
	{
		Buffer.SetNum(Need, EAllowShrinking::No);
	}
	Buffer[Cursor++] = V;
}

void FZeusPacketWriter::WriteUInt16(const uint16 V)
{
	const int32 Need = Cursor + 2;
	if (Buffer.Num() < Need)
	{
		Buffer.SetNum(Need, EAllowShrinking::No);
	}
	uint8* D = Buffer.GetData() + Cursor;
	D[0] = static_cast<uint8>(V & 0xFFu);
	D[1] = static_cast<uint8>((V >> 8) & 0xFFu);
	Cursor += 2;
}

void FZeusPacketWriter::WriteUInt32(const uint32 V)
{
	const int32 Need = Cursor + 4;
	if (Buffer.Num() < Need)
	{
		Buffer.SetNum(Need, EAllowShrinking::No);
	}
	uint8* D = Buffer.GetData() + Cursor;
	D[0] = static_cast<uint8>(V & 0xFFu);
	D[1] = static_cast<uint8>((V >> 8) & 0xFFu);
	D[2] = static_cast<uint8>((V >> 16) & 0xFFu);
	D[3] = static_cast<uint8>((V >> 24) & 0xFFu);
	Cursor += 4;
}

void FZeusPacketWriter::WriteUInt64(const uint64 V)
{
	WriteUInt32(static_cast<uint32>(V & 0xFFFFFFFFu));
	WriteUInt32(static_cast<uint32>((V >> 32) & 0xFFFFFFFFu));
}

void FZeusPacketWriter::WriteBytes(const void* Src, const int32 Len)
{
	if (Len <= 0)
	{
		return;
	}
	const int32 Need = Cursor + Len;
	if (Buffer.Num() < Need)
	{
		Buffer.SetNum(Need, EAllowShrinking::No);
	}
	FMemory::Memcpy(Buffer.GetData() + Cursor, Src, Len);
	Cursor += Len;
}

void FZeusPacketWriter::WriteStringUtf8(const FString& S)
{
	FTCHARToUTF8 Utf8(*S);
	const int32 ByteLen = Utf8.Length();
	if (ByteLen > 65535)
	{
		return;
	}
	WriteUInt16(static_cast<uint16>(ByteLen));
	if (ByteLen > 0)
	{
		WriteBytes(Utf8.Get(), ByteLen);
	}
}
