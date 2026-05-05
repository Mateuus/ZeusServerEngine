export class ZeusPacketReader {
  private offset = 0;

  constructor(
    private readonly data: Buffer,
    private readonly length: number,
  ) {}

  readUInt8(): number {
    if (this.offset + 1 > this.length) {
      throw new Error("PacketReader: unexpected end (u8)");
    }
    const v = this.data.readUInt8(this.offset);
    this.offset += 1;
    return v;
  }

  readUInt16(): number {
    if (this.offset + 2 > this.length) {
      throw new Error("PacketReader: unexpected end (u16)");
    }
    const v = this.data.readUInt16LE(this.offset);
    this.offset += 2;
    return v;
  }

  readUInt32(): number {
    if (this.offset + 4 > this.length) {
      throw new Error("PacketReader: unexpected end (u32)");
    }
    const v = this.data.readUInt32LE(this.offset);
    this.offset += 4;
    return v >>> 0;
  }

  readUInt64(): bigint {
    if (this.offset + 8 > this.length) {
      throw new Error("PacketReader: unexpected end (u64)");
    }
    const lo = BigInt(this.data.readUInt32LE(this.offset));
    const hi = BigInt(this.data.readUInt32LE(this.offset + 4));
    this.offset += 8;
    return (hi << 32n) | lo;
  }

  readStringUtf8(): string {
    const len = this.readUInt16();
    if (this.offset + len > this.length) {
      throw new Error("PacketReader: string past end");
    }
    const s = this.data.subarray(this.offset, this.offset + len).toString("utf8");
    this.offset += len;
    return s;
  }
}
