#pragma once

#include "CoreMinimal.h"
#include "ZeusNetworkTypes.h"
#include "ZeusOpcodes.h"
#include "ZeusPacketWriter.h"

class FSocket;
class FInternetAddr;

/**
 * Cliente UDP Zeus (não-UObject): socket, sequência, ACK e order id alinhados ao servidor / ClientTest.
 */
class ZEUSNETWORK_API FZeusUdpClient
{
public:
	FZeusUdpClient();
	~FZeusUdpClient();

	bool Open();
	void Close();
	bool IsOpen() const { return Socket != nullptr; }

	bool SetServerEndpoint(const FString& Host, int32 Port);

	bool SendWirePacket(const TArray<uint8>& WireBytes);

	/** Devolve um datagrama completo (cabeçalho+payload) se existir; non-blocking. */
	bool TryReceiveOne(TArray<uint8>& OutPacket);

	void ResetWireForNewHandshake();
	void ResetLoadingOrderForConnectResponse();

	void MarkReceivedFromServer(uint32 RemoteSequence);
	bool IsRemoteDuplicate(uint32 RemoteSequence) const;
	uint32 GetAck() const { return LastRemoteSeq; }
	uint32 GetAckBits() const { return AckBits; }
	uint32 NextOutboundSequence();

	uint16 ComputeFlags(EZeusNetChannel Channel, EZeusNetDelivery Delivery);

	void FinalizePacket(
		FZeusPacketWriter& PayloadWriter,
		EZeusOpcode Opcode,
		EZeusNetChannel Channel,
		EZeusNetDelivery Delivery,
		uint32 ConnId,
		TArray<uint8>& OutWire);

	uint64 ConnectClientNonce = 0;
	uint64 PendingServerChallenge = 0;
	uint32 ConnectionId = 0;
	uint64 SessionId = 0;

private:
	FSocket* Socket = nullptr;
	TSharedPtr<FInternetAddr> ServerAddr;

	uint32 NextSeq = 1;
	uint32 LastRemoteSeq = 0;
	uint32 AckBits = 0;
	uint16 OrderIdByChannel[5] = { 0, 0, 0, 0, 0 };
};
