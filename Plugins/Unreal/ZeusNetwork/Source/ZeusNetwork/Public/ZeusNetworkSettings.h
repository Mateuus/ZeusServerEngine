#pragma once

#include "ZeusNetwork.h"
#include "Engine/DeveloperSettings.h"
#include "ZeusNetworkSettings.generated.h"

/**
 * Configuração do cliente Zeus (Project Settings → Zeus Network).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Zeus Network"))
class ZEUSNETWORK_API UZeusNetworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UZeusNetworkSettings();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Zeus Network")
	FString DefaultHost;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Zeus Network")
	int32 DefaultPort;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Zeus Network")
	float ConnectTimeoutSeconds;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Zeus Network")
	float PingIntervalSeconds;
};
