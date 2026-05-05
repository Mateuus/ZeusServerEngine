import { magicToUInt32, ZEUS_MAX_PACKET_BYTES, ZEUS_PROTOCOL_VERSION, ZEUS_WIRE_HEADER_BYTES } from "./ZeusProtocol.js";

export interface ParsedHeader {
  magic: number;
  version: number;
  headerSize: number;
  opcode: number;
  channel: number;
  delivery: number;
  sequence: number;
  ack: number;
  ackBits: number;
  connectionId: number;
  payloadSize: number;
  flags: number;
}

export interface ParsedPacket {
  header: ParsedHeader;
  payload: Buffer;
}

export function parsePacket(datagram: Buffer): ParsedPacket {
  if (datagram.length < ZEUS_WIRE_HEADER_BYTES) {
    throw new Error("Packet too small");
  }
  const h: ParsedHeader = {
    magic: datagram.readUInt32LE(0) >>> 0,
    version: datagram.readUInt16LE(4),
    headerSize: datagram.readUInt16LE(6),
    opcode: datagram.readUInt16LE(8),
    channel: datagram.readUInt8(10),
    delivery: datagram.readUInt8(11),
    sequence: datagram.readUInt32LE(12) >>> 0,
    ack: datagram.readUInt32LE(16) >>> 0,
    ackBits: datagram.readUInt32LE(20) >>> 0,
    connectionId: datagram.readUInt32LE(24) >>> 0,
    payloadSize: datagram.readUInt16LE(28),
    flags: datagram.readUInt16LE(30),
  };
  const expected = h.headerSize + h.payloadSize;
  if (h.magic !== magicToUInt32()) {
    throw new Error("Bad magic");
  }
  if (h.version !== ZEUS_PROTOCOL_VERSION) {
    throw new Error("Bad version");
  }
  if (h.headerSize !== ZEUS_WIRE_HEADER_BYTES) {
    throw new Error("Bad headerSize");
  }
  if (expected > datagram.length || h.payloadSize > ZEUS_MAX_PACKET_BYTES - ZEUS_WIRE_HEADER_BYTES) {
    throw new Error("Bad payload size");
  }
  const payload = datagram.subarray(h.headerSize, h.headerSize + h.payloadSize);
  return { header: h, payload };
}
