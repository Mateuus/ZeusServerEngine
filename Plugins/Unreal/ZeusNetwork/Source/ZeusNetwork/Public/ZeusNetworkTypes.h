#pragma once

#include "ZeusNetwork.h"

UENUM(BlueprintType)
enum class EZeusConnectionState : uint8
{
	Disconnected UMETA(DisplayName = "Disconnected"),
	Connecting UMETA(DisplayName = "Connecting"),
	WaitingChallenge UMETA(DisplayName = "WaitingChallenge"),
	Connected UMETA(DisplayName = "Connected"),
	Disconnecting UMETA(DisplayName = "Disconnecting")
};

UENUM(BlueprintType)
enum class EZeusNetChannel : uint8
{
	Realtime = 0 UMETA(DisplayName = "Realtime"),
	Gameplay = 1 UMETA(DisplayName = "Gameplay"),
	Chat = 2 UMETA(DisplayName = "Chat"),
	Visual = 3 UMETA(DisplayName = "Visual"),
	Loading = 4 UMETA(DisplayName = "Loading")
};

UENUM(BlueprintType)
enum class EZeusNetDelivery : uint8
{
	Unreliable = 0 UMETA(DisplayName = "Unreliable"),
	UnreliableSequenced = 1 UMETA(DisplayName = "UnreliableSequenced"),
	Reliable = 2 UMETA(DisplayName = "Reliable"),
	ReliableOrdered = 3 UMETA(DisplayName = "ReliableOrdered")
};

#include "ZeusNetworkTypes.generated.h"
