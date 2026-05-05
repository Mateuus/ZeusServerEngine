/** Espelho de `Zeus::Protocol::PacketConstants` / opcodes (LE). */

export const ZEUS_PACKET_MAGIC =
  (BigInt("0x5a") << 0n) |
  (BigInt("0x45") << 8n) |
  (BigInt("0x55") << 16n) |
  (BigInt("0x53") << 24n);

export function magicToUInt32(): number {
  return Number(ZEUS_PACKET_MAGIC & 0xffffffffn) >>> 0;
}

export const ZEUS_PROTOCOL_VERSION = 2;
export const ZEUS_WIRE_HEADER_BYTES = 32;
export const ZEUS_MAX_PACKET_BYTES = 1200;

export const enum ENetChannel {
  Realtime = 0,
  Gameplay = 1,
  Chat = 2,
  Visual = 3,
  Loading = 4,
}

export const enum ENetDelivery {
  Unreliable = 0,
  UnreliableSequenced = 1,
  Reliable = 2,
  ReliableOrdered = 3,
}

export const enum EOpcode {
  C_CONNECT_REQUEST = 1000,
  S_CONNECT_OK = 1001,
  S_CONNECT_REJECT = 1002,
  S_CONNECT_CHALLENGE = 1003,
  C_CONNECT_RESPONSE = 1004,
  C_PING = 1010,
  S_PONG = 1011,
  C_DISCONNECT = 1020,
  S_DISCONNECT_OK = 1021,
  C_LOADING_FRAGMENT = 1030,
  S_LOADING_ASSEMBLED_OK = 1031,
}

export const enum ConnectRejectReason {
  InvalidProtocolVersion = 1,
  ServerFull = 2,
  InvalidPacket = 3,
}
