#pragma once

#include "CoreMinimal.h"

/** Espelho de Zeus::Protocol::EOpcode (servidor / ClientTest). */
enum class EZeusOpcode : uint16
{
	Unknown = 0,

	C_CONNECT_REQUEST = 1000,
	S_CONNECT_OK = 1001,
	S_CONNECT_REJECT = 1002,
	S_CONNECT_CHALLENGE = 1003,
	C_CONNECT_RESPONSE = 1004,

	C_PING = 1010,
	S_PONG = 1011,

	C_DISCONNECT = 1020,
	S_DISCONNECT_OK = 1021
};
