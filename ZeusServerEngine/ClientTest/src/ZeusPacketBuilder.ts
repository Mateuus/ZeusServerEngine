import {
  ENetChannel,
  ENetDelivery,
  magicToUInt32,
  ZEUS_PROTOCOL_VERSION,
  ZEUS_WIRE_HEADER_BYTES,
} from "./ZeusProtocol.js";
import { ZeusPacketWriter } from "./ZeusPacketWriter.js";

export interface PacketHeaderFields {
  opcode: number;
  channel: ENetChannel;
  delivery: ENetDelivery;
  sequence: number;
  ack: number;
  ackBits: number;
  connectionId: number;
  flags: number;
}

function writeHeaderAt(buf: Buffer, h: PacketHeaderFields, payloadSize: number): void {
  const magic = magicToUInt32();
  buf.writeUInt32LE(magic >>> 0, 0);
  buf.writeUInt16LE(ZEUS_PROTOCOL_VERSION, 4);
  buf.writeUInt16LE(ZEUS_WIRE_HEADER_BYTES, 6);
  buf.writeUInt16LE(h.opcode & 0xffff, 8);
  buf.writeUInt8(h.channel & 0xff, 10);
  buf.writeUInt8(h.delivery & 0xff, 11);
  buf.writeUInt32LE(h.sequence >>> 0, 12);
  buf.writeUInt32LE(h.ack >>> 0, 16);
  buf.writeUInt32LE(h.ackBits >>> 0, 20);
  buf.writeUInt32LE(h.connectionId >>> 0, 24);
  buf.writeUInt16LE(payloadSize & 0xffff, 28);
  buf.writeUInt16LE(h.flags & 0xffff, 30);
}

export class ZeusPacketBuilder {
  private readonly writer = new ZeusPacketWriter();

  reset(): void {
    this.writer.clear();
    this.writer.setCursor(ZEUS_WIRE_HEADER_BYTES);
  }

  payloadWriter(): ZeusPacketWriter {
    return this.writer;
  }

  finalize(header: PacketHeaderFields): Buffer {
    const end = this.writer.getCursor();
    if (end < ZEUS_WIRE_HEADER_BYTES) {
      throw new Error("PacketBuilder: size < header");
    }
    const payloadSize = end - ZEUS_WIRE_HEADER_BYTES;
    const buf = this.writer.buffer();
    writeHeaderAt(buf, header, payloadSize);
    return buf.subarray(0, end);
  }
}
