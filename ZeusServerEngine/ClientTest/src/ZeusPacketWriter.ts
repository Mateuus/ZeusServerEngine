import { ZEUS_MAX_PACKET_BYTES } from "./ZeusProtocol.js";

function writeU16LE(buf: Buffer, offset: number, v: number): void {
  buf.writeUInt16LE(v >>> 0, offset);
}

function writeU32LE(buf: Buffer, offset: number, v: number): void {
  buf.writeUInt32LE(v >>> 0, offset);
}

function writeU64LE(buf: Buffer, offset: number, v: bigint): void {
  const lo = Number(v & 0xffffffffn);
  const hi = Number((v >> 32n) & 0xffffffffn);
  buf.writeUInt32LE(lo >>> 0, offset);
  buf.writeUInt32LE(hi >>> 0, offset + 4);
}

export class ZeusPacketWriter {
  private buf = Buffer.alloc(ZEUS_MAX_PACKET_BYTES);
  private cursor = 0;

  clear(): void {
    this.cursor = 0;
  }

  setCursor(pos: number): void {
    this.cursor = pos;
  }

  getCursor(): number {
    return this.cursor;
  }

  ensure(n: number): void {
    if (this.cursor + n > ZEUS_MAX_PACKET_BYTES) {
      throw new Error("PacketWriter: exceeded max packet size");
    }
  }

  writeUInt8(v: number): void {
    this.ensure(1);
    this.buf.writeUInt8(v & 0xff, this.cursor);
    this.cursor += 1;
  }

  writeUInt16(v: number): void {
    this.ensure(2);
    writeU16LE(this.buf, this.cursor, v);
    this.cursor += 2;
  }

  writeUInt32(v: number): void {
    this.ensure(4);
    writeU32LE(this.buf, this.cursor, v);
    this.cursor += 4;
  }

  writeUInt64(v: bigint): void {
    this.ensure(8);
    writeU64LE(this.buf, this.cursor, v);
    this.cursor += 8;
  }

  writeStringUtf8(s: string): void {
    const b = Buffer.from(s, "utf8");
    if (b.length > 65535) {
      throw new Error("string too long");
    }
    this.writeUInt16(b.length);
    this.ensure(b.length);
    b.copy(this.buf, this.cursor);
    this.cursor += b.length;
  }

  writeBytes(data: Buffer): void {
    this.ensure(data.length);
    data.copy(this.buf, this.cursor);
    this.cursor += data.length;
  }

  slice(): Buffer {
    return this.buf.subarray(0, this.cursor);
  }

  buffer(): Buffer {
    return this.buf;
  }
}
