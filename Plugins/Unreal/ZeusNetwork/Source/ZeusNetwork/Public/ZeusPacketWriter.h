#pragma once

#include "CoreMinimal.h"

/** Escrita binária LE para o wire Zeus (sem FArchive). */
class ZEUSNETWORK_API FZeusPacketWriter
{
public:
	void Reset();
	void Clear();

	void WriteUInt8(uint8 V);
	void WriteUInt16(uint16 V);
	void WriteUInt32(uint32 V);
	void WriteUInt64(uint64 V);
	void WriteBytes(const void* Src, int32 Len);
	void WriteStringUtf8(const FString& S);

	const TArray<uint8>& GetBuffer() const { return Buffer; }
	TArray<uint8>& GetBufferMutable() { return Buffer; }

	int32 Tell() const { return Cursor; }
	void Seek(const int32 Pos) { Cursor = Pos; }
	void SetSize(const int32 NewSize);

private:
	TArray<uint8> Buffer;
	int32 Cursor = 0;
};
