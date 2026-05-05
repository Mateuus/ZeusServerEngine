#pragma once

#include "Delegates/Delegate.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ZeusNetwork.h"
#include "ZeusNetworkTypes.h"
#include "ZeusUdpClient.h"

#include "ZeusNetworkSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FZeusOnConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeusOnConnectionFailed, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FZeusOnDisconnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeusOnPingUpdated, float, RttMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeusOnNetworkLog, FString, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZeusOnServerTravelRequested, const FString&, MapName, const FString&, MapPath);

UCLASS()
class ZEUSNETWORK_API UZeusNetworkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Obtém o subsistema Zeus a partir de um World Context.
	 * Não funciona durante **Event Init** da Game Instance: o motor chama ReceiveInit antes de criar UGameInstanceSubsystem.
	 * Usa **Delay(0)** após Event Init, ou lógica em **BeginPlay** (ex. GameMode), ou este nó noutro momento em que a GI já tenha subsistemas.
	 */
	UFUNCTION(BlueprintPure, Category = "Zeus|Network", meta = (WorldContext = "WorldContextObject"))
	static UZeusNetworkSubsystem* GetZeusNetworkSubsystem(const UObject* WorldContextObject);

	virtual ~UZeusNetworkSubsystem() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Zeus|Network")
	void ConnectToZeusServer(const FString& Host, int32 Port);

	UFUNCTION(BlueprintCallable, Category = "Zeus|Network")
	void ConnectToDefaultZeusServer();

	UFUNCTION(BlueprintCallable, Category = "Zeus|Network")
	void DisconnectFromZeusServer();

	UFUNCTION(BlueprintCallable, Category = "Zeus|Network")
	void SendPing();

	UFUNCTION(BlueprintPure, Category = "Zeus|Network")
	EZeusConnectionState GetConnectionState() const { return ConnectionState; }

	UFUNCTION(BlueprintPure, Category = "Zeus|Network")
	bool IsConnected() const { return ConnectionState == EZeusConnectionState::Connected; }

	UFUNCTION(BlueprintPure, Category = "Zeus|Network")
	int32 GetConnectionId() const { return static_cast<int32>(ConnectionId); }

	UFUNCTION(BlueprintPure, Category = "Zeus|Network")
	int64 GetSessionId() const { return static_cast<int64>(SessionId); }

	UFUNCTION(BlueprintPure, Category = "Zeus|Network")
	float GetLastRttMs() const { return LastRttMs; }

	UPROPERTY(BlueprintAssignable, Category = "Zeus|Network")
	FZeusOnConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Zeus|Network")
	FZeusOnConnectionFailed OnConnectionFailed;

	UPROPERTY(BlueprintAssignable, Category = "Zeus|Network")
	FZeusOnDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Zeus|Network")
	FZeusOnPingUpdated OnPingUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Zeus|Network")
	FZeusOnNetworkLog OnNetworkLog;

	/** Disparado quando o servidor envia S_TRAVEL_TO_MAP (logo apos o handshake). MapPath e o caminho Unreal completo (ex.: "/Game/ThirdPerson/TestWorld"). */
	UPROPERTY(BlueprintAssignable, Category = "Zeus|Network")
	FZeusOnServerTravelRequested OnServerTravelRequested;

private:
	bool PollTick(float DeltaTime);

	void EmitLog(const FString& MessageBody);
	void AppendSessionLogLine(const FString& FullLine);
	void OpenSessionLogIfNeeded();
	void CloseSessionLog();

	void HandleIncomingPacket(const TArray<uint8>& Wire);
	void FailConnect(const FString& Reason);
	void CompleteDisconnectLocal();

	void SendConnectRequestInternal();
	void SendConnectResponseInternal();
	void SendPingInternal();
	void SendDisconnectInternal();

	TUniquePtr<FZeusUdpClient> Udp;

	FTSTicker::FDelegateHandle PollHandle;

	EZeusConnectionState ConnectionState = EZeusConnectionState::Disconnected;

	uint32 ConnectionId = 0;
	uint64 SessionId = 0;
	uint64 ServerTimeMs = 0;
	uint32 HeartbeatIntervalMs = 5000;

	float LastRttMs = 0.f;
	float PingAccumulator = 0.f;

	double ConnectDeadlineMonotonic = 0.0;
	double DisconnectDeadlineMonotonic = 0.0;
	double LastConnectResponseSentMonotonic = 0.0;

	FString SessionLogPath;
	bool bSessionLogOpened = false;
};
