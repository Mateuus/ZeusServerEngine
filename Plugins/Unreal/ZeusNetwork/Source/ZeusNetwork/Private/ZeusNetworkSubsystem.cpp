#include "ZeusNetworkSubsystem.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"

#include "ZeusNetworkLog.h"
#include "ZeusNetworkSettings.h"
#include "ZeusOpcodes.h"
#include "ZeusPacketHeader.h"
#include "ZeusPacketReader.h"
#include "ZeusPacketWriter.h"
#include "ZeusUdpClient.h"

#include <chrono>

namespace ZeusNetSubsystemDetail
{
	static uint64 UnixUtcMillis()
	{
		using namespace std::chrono;
		return static_cast<uint64>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
	}
}

UZeusNetworkSubsystem::~UZeusNetworkSubsystem() = default;

UZeusNetworkSubsystem* UZeusNetworkSubsystem::GetZeusNetworkSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !IsValid(WorldContextObject))
	{
		return nullptr;
	}
	UObject* MutableCtx = const_cast<UObject*>(WorldContextObject);
	if (UGameInstance* DirectGi = Cast<UGameInstance>(MutableCtx))
	{
		// ReceiveInit (Event Init) corre antes de SubsystemCollection.Initialize em UGameInstance::Init;
		// nessa fase GetSubsystem<UZeusNetworkSubsystem>() devolve nullptr.
		return DirectGi->GetSubsystem<UZeusNetworkSubsystem>();
	}
	if (UGameInstance* Gi = UGameplayStatics::GetGameInstance(MutableCtx))
	{
		return Gi->GetSubsystem<UZeusNetworkSubsystem>();
	}
	return nullptr;
}

void UZeusNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UZeusNetworkSubsystem::PollTick));
}

void UZeusNetworkSubsystem::Deinitialize()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PollHandle);
		PollHandle.Reset();
	}
	CloseSessionLog();
	// Se Shutdown não correu, ainda podemos estar Connected sem C_DISCONNECT no wire.
	if (Udp && Udp->IsOpen() && ConnectionState == EZeusConnectionState::Connected)
	{
		SendDisconnectInternal();
	}
	if (Udp)
	{
		Udp->Close();
		Udp.Reset();
	}
	ConnectionState = EZeusConnectionState::Disconnected;
	Super::Deinitialize();
}

void UZeusNetworkSubsystem::EmitLog(const FString& MessageBody)
{
	const FString Full = FString::Printf(TEXT("[ZeusNetwork] %s"), *MessageBody);
	UE_LOG(LogZeusNetwork, Log, TEXT("%s"), *Full);
	OnNetworkLog.Broadcast(Full);
	AppendSessionLogLine(Full);
}

void UZeusNetworkSubsystem::OpenSessionLogIfNeeded()
{
	if (bSessionLogOpened)
	{
		return;
	}
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("ZeusNetwork"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	SessionLogPath = FPaths::Combine(Dir, TEXT("session.log"));
	bSessionLogOpened = true;
	const FString StartLine = FString::Printf(
		TEXT("--- ZeusNetwork session start (UTC) %s ---"),
		*FDateTime::UtcNow().ToIso8601());
	AppendSessionLogLine(StartLine);
}

void UZeusNetworkSubsystem::CloseSessionLog()
{
	bSessionLogOpened = false;
	SessionLogPath.Empty();
}

void UZeusNetworkSubsystem::AppendSessionLogLine(const FString& FullLine)
{
	if (!bSessionLogOpened || SessionLogPath.IsEmpty())
	{
		return;
	}
	FArchive* const Writer = IFileManager::Get().CreateFileWriter(*SessionLogPath, FILEWRITE_Append);
	if (!Writer)
	{
		return;
	}
	const FTCHARToUTF8 Utf8(*FullLine);
	const FTCHARToUTF8 Nl(*FString(LINE_TERMINATOR));
	Writer->Serialize((void*)Utf8.Get(), Utf8.Length());
	Writer->Serialize((void*)Nl.Get(), Nl.Length());
	delete Writer;
}

bool UZeusNetworkSubsystem::PollTick(const float DeltaTime)
{
	if (Udp && Udp->IsOpen())
	{
		TArray<uint8> Packet;
		while (Udp->TryReceiveOne(Packet))
		{
			HandleIncomingPacket(Packet);
		}
	}

	const double Now = FPlatformTime::Seconds();

	if (ConnectionState == EZeusConnectionState::Connecting || ConnectionState == EZeusConnectionState::WaitingChallenge)
	{
		if (Now >= ConnectDeadlineMonotonic)
		{
			FailConnect(TEXT("Connect timeout"));
		}
		else if (ConnectionState == EZeusConnectionState::WaitingChallenge && (Now - LastConnectResponseSentMonotonic) > 2.0)
		{
			EmitLog(TEXT("Re-sending C_CONNECT_RESPONSE (handshake retry)"));
			SendConnectResponseInternal();
			LastConnectResponseSentMonotonic = Now;
		}
	}

	if (ConnectionState == EZeusConnectionState::Disconnecting)
	{
		if (Now >= DisconnectDeadlineMonotonic)
		{
			EmitLog(TEXT("Disconnect timeout; closing socket"));
			CompleteDisconnectLocal();
		}
	}

	if (ConnectionState == EZeusConnectionState::Connected)
	{
		const UZeusNetworkSettings* S = GetDefault<UZeusNetworkSettings>();
		if (S && S->PingIntervalSeconds > KINDA_SMALL_NUMBER)
		{
			PingAccumulator += DeltaTime;
			if (PingAccumulator >= S->PingIntervalSeconds)
			{
				PingAccumulator = 0.f;
				SendPingInternal();
			}
		}
	}

	return true;
}

void UZeusNetworkSubsystem::ConnectToDefaultZeusServer()
{
	if (const UZeusNetworkSettings* S = GetDefault<UZeusNetworkSettings>())
	{
		ConnectToZeusServer(S->DefaultHost, S->DefaultPort);
	}
}

void UZeusNetworkSubsystem::ConnectToZeusServer(const FString& Host, const int32 Port)
{
	if (ConnectionState == EZeusConnectionState::Connecting
		|| ConnectionState == EZeusConnectionState::WaitingChallenge
		|| ConnectionState == EZeusConnectionState::Connected
		|| ConnectionState == EZeusConnectionState::Disconnecting)
	{
		EmitLog(TEXT("Connect ignored: already active; call Disconnect first"));
		return;
	}

	OpenSessionLogIfNeeded();

	Udp = MakeUnique<FZeusUdpClient>();
	Udp->ResetWireForNewHandshake();

	if (!Udp->Open())
	{
		Udp.Reset();
		FailConnect(TEXT("Failed to open UDP socket"));
		return;
	}

	if (!Udp->SetServerEndpoint(Host, Port))
	{
		Udp->Close();
		Udp.Reset();
		FailConnect(TEXT("Invalid server host/port"));
		return;
	}

	const UZeusNetworkSettings* S = GetDefault<UZeusNetworkSettings>();
	const float TimeoutSec = S ? FMath::Max(1.f, S->ConnectTimeoutSeconds) : 30.f;
	ConnectDeadlineMonotonic = FPlatformTime::Seconds() + static_cast<double>(TimeoutSec);

	EmitLog(TEXT("Opening UDP socket"));
	ConnectionState = EZeusConnectionState::Connecting;
	SendConnectRequestInternal();
}

void UZeusNetworkSubsystem::SendConnectRequestInternal()
{
	if (!Udp)
	{
		return;
	}

	FZeusPacketWriter W;
	W.SetSize(ZeusNetWire::ZeusWireHeaderBytes);
	W.Seek(ZeusNetWire::ZeusWireHeaderBytes);
	W.WriteUInt16(ZeusNetWire::ZeusProtocolVersion);
	W.WriteStringUtf8(TEXT("ZClientMMO-Unreal-Dev"));
	Udp->ConnectClientNonce = (static_cast<uint64>(FMath::Rand()) << 32) ^ static_cast<uint64>(FPlatformTime::Cycles64());
	W.WriteUInt64(Udp->ConnectClientNonce);

	TArray<uint8> Wire;
	Udp->FinalizePacket(W, EZeusOpcode::C_CONNECT_REQUEST, EZeusNetChannel::Loading, EZeusNetDelivery::Reliable, 0, Wire);
	if (Wire.Num() > 0 && Udp->SendWirePacket(Wire))
	{
		EmitLog(TEXT("Sending C_CONNECT_REQUEST"));
	}
	else
	{
		FailConnect(TEXT("Failed to send C_CONNECT_REQUEST"));
	}
}

void UZeusNetworkSubsystem::SendConnectResponseInternal()
{
	if (!Udp || Udp->ConnectionId == 0)
	{
		return;
	}

	Udp->ResetLoadingOrderForConnectResponse();

	FZeusPacketWriter W;
	W.SetSize(ZeusNetWire::ZeusWireHeaderBytes);
	W.Seek(ZeusNetWire::ZeusWireHeaderBytes);
	W.WriteUInt64(Udp->ConnectClientNonce);
	W.WriteUInt64(Udp->PendingServerChallenge);

	TArray<uint8> Wire;
	Udp->FinalizePacket(
		W,
		EZeusOpcode::C_CONNECT_RESPONSE,
		EZeusNetChannel::Loading,
		EZeusNetDelivery::ReliableOrdered,
		Udp->ConnectionId,
		Wire);
	if (Wire.Num() > 0)
	{
		Udp->SendWirePacket(Wire);
		EmitLog(TEXT("Sending C_CONNECT_RESPONSE"));
	}
}

void UZeusNetworkSubsystem::SendPingInternal()
{
	if (!Udp || ConnectionState != EZeusConnectionState::Connected)
	{
		return;
	}

	FZeusPacketWriter W;
	W.SetSize(ZeusNetWire::ZeusWireHeaderBytes);
	W.Seek(ZeusNetWire::ZeusWireHeaderBytes);
	const uint64 T = ZeusNetSubsystemDetail::UnixUtcMillis();
	W.WriteUInt64(T);

	TArray<uint8> Wire;
	Udp->FinalizePacket(W, EZeusOpcode::C_PING, EZeusNetChannel::Gameplay, EZeusNetDelivery::Unreliable, Udp->ConnectionId, Wire);
	if (Wire.Num() > 0 && Udp->SendWirePacket(Wire))
	{
		EmitLog(TEXT("Sending C_PING"));
	}
}

void UZeusNetworkSubsystem::SendDisconnectInternal()
{
	if (!Udp || ConnectionState != EZeusConnectionState::Connected)
	{
		return;
	}

	FZeusPacketWriter W;
	W.SetSize(ZeusNetWire::ZeusWireHeaderBytes);
	W.Seek(ZeusNetWire::ZeusWireHeaderBytes);
	W.WriteUInt16(0);

	TArray<uint8> Wire;
	Udp->FinalizePacket(W, EZeusOpcode::C_DISCONNECT, EZeusNetChannel::Gameplay, EZeusNetDelivery::Reliable, Udp->ConnectionId, Wire);
	if (Wire.Num() > 0)
	{
		Udp->SendWirePacket(Wire);
		EmitLog(TEXT("Disconnecting"));
	}
}

void UZeusNetworkSubsystem::SendPing()
{
	SendPingInternal();
}

void UZeusNetworkSubsystem::DisconnectFromZeusServer()
{
	if (!Udp || !Udp->IsOpen())
	{
		ConnectionState = EZeusConnectionState::Disconnected;
		return;
	}

	if (ConnectionState == EZeusConnectionState::Connected)
	{
		// SendDisconnectInternal exige Connected; não mudar o estado antes do envio.
		SendDisconnectInternal();
		ConnectionState = EZeusConnectionState::Disconnecting;
		const UZeusNetworkSettings* S = GetDefault<UZeusNetworkSettings>();
		const float T = S ? FMath::Max(1.f, S->ConnectTimeoutSeconds) : 10.f;
		DisconnectDeadlineMonotonic = FPlatformTime::Seconds() + static_cast<double>(T);
	}
	else if (
		ConnectionState == EZeusConnectionState::Connecting
		|| ConnectionState == EZeusConnectionState::WaitingChallenge)
	{
		EmitLog(TEXT("Disconnect (handshake aborted)"));
		CompleteDisconnectLocal();
	}
	else
	{
		CompleteDisconnectLocal();
	}
}

void UZeusNetworkSubsystem::FailConnect(const FString& Reason)
{
	EmitLog(FString::Printf(TEXT("Connection failed: %s"), *Reason));
	OnConnectionFailed.Broadcast(Reason);
	if (Udp)
	{
		Udp->Close();
		Udp.Reset();
	}
	ConnectionState = EZeusConnectionState::Disconnected;
}

void UZeusNetworkSubsystem::CompleteDisconnectLocal()
{
	if (Udp)
	{
		Udp->Close();
		Udp.Reset();
	}
	ConnectionState = EZeusConnectionState::Disconnected;
	ConnectionId = 0;
	SessionId = 0;
	EmitLog(TEXT("Disconnected"));
	OnDisconnected.Broadcast();
}

void UZeusNetworkSubsystem::HandleIncomingPacket(const TArray<uint8>& Wire)
{
	if (!Udp)
	{
		return;
	}

	FZeusPacketHeader H;
	if (!FZeusPacketHeader::TryRead(Wire.GetData(), Wire.Num(), H))
	{
		UE_LOG(LogZeusNetwork, Warning, TEXT("[ZeusNetwork] Dropped datagram: invalid header"));
		return;
	}

	Udp->MarkReceivedFromServer(H.Sequence);

	const uint8* PayloadPtr = Wire.GetData() + ZeusNetWire::ZeusWireHeaderBytes;
	const int32 PayloadLen = static_cast<int32>(H.PayloadSize);
	FZeusPacketReader Reader(PayloadPtr, PayloadLen);

	const EZeusOpcode Op = static_cast<EZeusOpcode>(H.Opcode);

	switch (Op)
	{
	case EZeusOpcode::S_CONNECT_CHALLENGE:
	{
		uint64 ServerNonce = 0;
		uint32 Conn = 0;
		if (!Reader.ReadUInt64(ServerNonce) || !Reader.ReadUInt32(Conn))
		{
			UE_LOG(LogZeusNetwork, Warning, TEXT("[ZeusNetwork] Invalid S_CONNECT_CHALLENGE payload"));
			return;
		}
		Udp->PendingServerChallenge = ServerNonce;
		Udp->ConnectionId = Conn;
		EmitLog(FString::Printf(TEXT("Received S_CONNECT_CHALLENGE connectionId=%u"), Conn));
		ConnectionState = EZeusConnectionState::WaitingChallenge;
		SendConnectResponseInternal();
		LastConnectResponseSentMonotonic = FPlatformTime::Seconds();
		break;
	}
	case EZeusOpcode::S_CONNECT_OK:
	{
		uint32 Conn = 0;
		uint64 Sess = 0;
		uint64 SrvTime = 0;
		uint32 Heart = 0;
		if (!Reader.ReadUInt32(Conn) || !Reader.ReadUInt64(Sess) || !Reader.ReadUInt64(SrvTime) || !Reader.ReadUInt32(Heart))
		{
			UE_LOG(LogZeusNetwork, Warning, TEXT("[ZeusNetwork] Invalid S_CONNECT_OK payload"));
			return;
		}
		Udp->ConnectionId = Conn;
		ConnectionId = Conn;
		SessionId = Sess;
		ServerTimeMs = SrvTime;
		HeartbeatIntervalMs = Heart;
		const bool bAlreadyConnected = (ConnectionState == EZeusConnectionState::Connected);
		ConnectionState = EZeusConnectionState::Connected;
		PingAccumulator = 0.f;
		if (!bAlreadyConnected)
		{
			EmitLog(FString::Printf(
				TEXT("Connected connectionId=%u sessionId=%llu serverTimeMs=%llu heartbeatIntervalMs=%u"),
				Conn,
				static_cast<unsigned long long>(Sess),
				static_cast<unsigned long long>(SrvTime),
				Heart));
			OnConnected.Broadcast();
			// O servidor confirma S_CONNECT_OK com reliable + wall-clock (reenvios ~3s). O ping periódico
			// usa DeltaTime do jogo; em PIE/slomo/ticks lentos o primeiro C_PING pode chegar tarde demais e o
			// servidor faz give-up. Um ping imediato carrega Ack=última sequência do servidor e liberta a fila.
			SendPingInternal();
		}
		break;
	}
	case EZeusOpcode::S_CONNECT_REJECT:
	{
		uint16 Code = 0;
		FString Msg;
		if (!Reader.ReadUInt16(Code) || !Reader.ReadStringUtf8(Msg))
		{
			Msg = TEXT("Invalid reject payload");
		}
		FailConnect(FString::Printf(TEXT("S_CONNECT_REJECT code=%u msg=%s"), Code, *Msg));
		break;
	}
	case EZeusOpcode::S_PONG:
	{
		if (ConnectionState != EZeusConnectionState::Connected)
		{
			return;
		}
		uint64 ClientTimeMs = 0;
		uint64 SrvTime = 0;
		if (!Reader.ReadUInt64(ClientTimeMs) || !Reader.ReadUInt64(SrvTime))
		{
			return;
		}
		const uint64 NowMs = ZeusNetSubsystemDetail::UnixUtcMillis();
		const float Rtt = static_cast<float>(static_cast<int64>(NowMs) - static_cast<int64>(ClientTimeMs));
		LastRttMs = FMath::Max(0.f, Rtt);
		EmitLog(FString::Printf(TEXT("S_PONG RTT=%.2fms"), LastRttMs));
		OnPingUpdated.Broadcast(LastRttMs);
		break;
	}
	case EZeusOpcode::S_DISCONNECT_OK:
	{
		uint64 SrvTime = 0;
		Reader.ReadUInt64(SrvTime);
		EmitLog(FString::Printf(
			TEXT("S_DISCONNECT_OK serverTimeMs=%llu"),
			static_cast<unsigned long long>(SrvTime)));
		CompleteDisconnectLocal();
		break;
	}
	case EZeusOpcode::S_TRAVEL_TO_MAP:
	{
		FString MapName;
		FString MapPath;
		uint64 SrvTime = 0;
		if (!Reader.ReadStringUtf8(MapName) || !Reader.ReadStringUtf8(MapPath) || !Reader.ReadUInt64(SrvTime))
		{
			UE_LOG(LogZeusNetwork, Warning, TEXT("[ZeusNetwork] Invalid S_TRAVEL_TO_MAP payload"));
			return;
		}
		EmitLog(FString::Printf(TEXT("S_TRAVEL_TO_MAP map=%s path=%s"), *MapName, *MapPath));
		OnServerTravelRequested.Broadcast(MapName, MapPath);
		break;
	}
	default:
		break;
	}
}
