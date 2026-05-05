#pragma once

#include "CoreMinimal.h"

/** Leitura binária LE com limites (não lê para além do payload). */
class ZEUSNETWORK_API FZeusPacketReader
{
public:
	FZeusPacketReader(const uint8* Data, int32 PayloadLen);

	bool ReadUInt8(uint8& OutV);
	bool ReadUInt16(uint16& OutV);
	bool ReadUInt32(uint32& OutV);
	bool ReadUInt64(uint64& OutV);
	bool ReadBytes(void* Dest, int32 Len);
	bool ReadStringUtf8(FString& OutS);

	int32 Tell() const { return Cursor; }
	int32 GetPayloadLen() const { return PayloadLen; }

private:
	const uint8* const Data;
	const int32 PayloadLen;
	int32 Cursor = 0;
};
