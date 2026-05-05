#include "ZeusNetworkSettings.h"

UZeusNetworkSettings::UZeusNetworkSettings()
{
	DefaultHost = TEXT("127.0.0.1");
	DefaultPort = 27777;
	ConnectTimeoutSeconds = 30.f;
	PingIntervalSeconds = 1.f;
}
