#include "ZeusUdpClient.h"

#include "Common/UdpSocketBuilder.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#include "ZeusPacketHeader.h"

FZeusUdpClient::FZeusUdpClient() = default;

FZeusUdpClient::~FZeusUdpClient()
{
	Close();
}

bool FZeusUdpClient::Open()
{
	Close();

	FSocket* NewSocket = FUdpSocketBuilder(TEXT("ZeusNetworkUdp"))
		.AsReusable()
		.BoundToPort(0)
		.WithReceiveBufferSize(256 * 1024)
		.WithSendBufferSize(256 * 1024)
		.Build();

	if (!NewSocket)
	{
		return false;
	}

	NewSocket->SetNonBlocking(true);
	Socket = NewSocket;
	return true;
}

void FZeusUdpClient::Close()
{
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
	ServerAddr.Reset();
}

bool FZeusUdpClient::SetServerEndpoint(const FString& Host, const int32 Port)
{
	ISocketSubsystem* Subsys = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsys)
	{
		return false;
	}

	TSharedRef<FInternetAddr> Addr = Subsys->CreateInternetAddr();
	bool bIsValid = false;
	Addr->SetIp(*Host, bIsValid);
	if (!bIsValid)
	{
		return false;
	}
	Addr->SetPort(Port);
	ServerAddr = Addr;
	return true;
}

bool FZeusUdpClient::SendWirePacket(const TArray<uint8>& WireBytes)
{
	if (!Socket || !ServerAddr.IsValid() || WireBytes.Num() <= 0)
	{
		return false;
	}

	int32 Sent = 0;
	return Socket->SendTo(WireBytes.GetData(), WireBytes.Num(), Sent, *ServerAddr) && Sent == WireBytes.Num();
}

bool FZeusUdpClient::TryReceiveOne(TArray<uint8>& OutPacket)
{
	OutPacket.Reset();
	if (!Socket)
	{
		return false;
	}

	ISocketSubsystem* Subsys = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsys)
	{
		return false;
	}

	TArray<uint8> Buf;
	Buf.SetNumUninitialized(ZeusNetWire::ZeusMaxPacketBytes);

	TSharedRef<FInternetAddr> From = Subsys->CreateInternetAddr();
	int32 Read = 0;
	if (!Socket->RecvFrom(Buf.GetData(), Buf.Num(), Read, *From, ESocketReceiveFlags::None))
	{
		const ESocketErrors Err = Subsys->GetLastErrorCode();
		if (Err == SE_EWOULDBLOCK || Err == SE_NO_ERROR)
		{
			return false;
		}
		return false;
	}
	if (Read < ZeusNetWire::ZeusWireHeaderBytes)
	{
		return false;
	}

	OutPacket.SetNum(Read, EAllowShrinking::No);
	FMemory::Memcpy(OutPacket.GetData(), Buf.GetData(), Read);
	return true;
}

void FZeusUdpClient::ResetWireForNewHandshake()
{
	NextSeq = 1;
	LastRemoteSeq = 0;
	AckBits = 0;
	for (int32 I = 0; I < 5; ++I)
	{
		OrderIdByChannel[I] = 0;
	}
	ConnectClientNonce = 0;
	PendingServerChallenge = 0;
	ConnectionId = 0;
	SessionId = 0;
}

void FZeusUdpClient::ResetLoadingOrderForConnectResponse()
{
	OrderIdByChannel[static_cast<int32>(EZeusNetChannel::Loading)] = 0;
}

void FZeusUdpClient::MarkReceivedFromServer(const uint32 RemoteSequence)
{
	if (RemoteSequence > LastRemoteSeq)
	{
		const uint32 Delta = RemoteSequence - LastRemoteSeq;
		if (Delta >= 33u)
		{
			LastRemoteSeq = RemoteSequence;
			AckBits = 0;
		}
		else
		{
			AckBits = (AckBits << Delta);
			LastRemoteSeq = RemoteSequence;
		}
	}
	else if (RemoteSequence < LastRemoteSeq)
	{
		const uint32 Behind = LastRemoteSeq - RemoteSequence;
		if (Behind > 0 && Behind <= 32u)
		{
			AckBits |= (1u << (Behind - 1));
		}
	}
}

bool FZeusUdpClient::IsRemoteDuplicate(const uint32 RemoteSequence) const
{
	if (RemoteSequence > LastRemoteSeq)
	{
		return false;
	}
	if (RemoteSequence == LastRemoteSeq)
	{
		return true;
	}
	const uint32 Behind = LastRemoteSeq - RemoteSequence;
	if (Behind == 0 || Behind > 32u)
	{
		return true;
	}
	const uint32 Mask = 1u << (Behind - 1);
	return (AckBits & Mask) != 0;
}

uint32 FZeusUdpClient::NextOutboundSequence()
{
	return NextSeq++;
}

uint16 FZeusUdpClient::ComputeFlags(const EZeusNetChannel Channel, const EZeusNetDelivery Delivery)
{
	const bool bUseOrdered =
		Delivery == EZeusNetDelivery::ReliableOrdered || Delivery == EZeusNetDelivery::UnreliableSequenced;
	if (!bUseOrdered)
	{
		return 0;
	}
	const int32 Ch = static_cast<int32>(Channel);
	if (Ch < 0 || Ch >= 5)
	{
		return 0;
	}
	const uint16 V = OrderIdByChannel[Ch] & 0xffffu;
	OrderIdByChannel[Ch] = static_cast<uint16>((OrderIdByChannel[Ch] + 1) & 0xffffu);
	return V;
}

void FZeusUdpClient::FinalizePacket(
	FZeusPacketWriter& PayloadWriter,
	const EZeusOpcode Opcode,
	const EZeusNetChannel Channel,
	const EZeusNetDelivery Delivery,
	const uint32 ConnId,
	TArray<uint8>& OutWire)
{
	TArray<uint8>& Buf = PayloadWriter.GetBufferMutable();
	const int32 End = PayloadWriter.Tell();
	if (End < ZeusNetWire::ZeusWireHeaderBytes)
	{
		OutWire.Reset();
		return;
	}

	const int32 PayloadSize = End - ZeusNetWire::ZeusWireHeaderBytes;
	if (PayloadSize < 0 || End > ZeusNetWire::ZeusMaxPacketBytes)
	{
		OutWire.Reset();
		return;
	}

	const uint16 Flags = ComputeFlags(Channel, Delivery);
	const uint32 Seq = NextOutboundSequence();

	FZeusPacketHeader H;
	H.Magic = ZeusNetWire::ZeusMagicUInt32();
	H.Version = ZeusNetWire::ZeusProtocolVersion;
	H.HeaderSize = ZeusNetWire::ZeusWireHeaderBytes;
	H.Opcode = static_cast<uint16>(Opcode);
	H.Channel = static_cast<uint8>(Channel);
	H.Delivery = static_cast<uint8>(Delivery);
	H.Sequence = Seq;
	H.Ack = GetAck();
	H.AckBits = GetAckBits();
	H.ConnectionId = ConnId;
	H.PayloadSize = static_cast<uint16>(PayloadSize);
	H.Flags = Flags;

	H.WriteBytes(Buf.GetData());
	OutWire = MoveTemp(Buf);
}
