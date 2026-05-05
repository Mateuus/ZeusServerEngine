import * as dgram from "node:dgram";
import * as readline from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";
import {
  ENetChannel,
  ENetDelivery,
  EOpcode,
  ZEUS_PROTOCOL_VERSION,
} from "./ZeusProtocol.js";
import { ZeusPacketBuilder } from "./ZeusPacketBuilder.js";
import { ZeusPacketReader } from "./ZeusPacketReader.js";
import { parsePacket } from "./ZeusPacketParser.js";

interface CliOptions {
  host: string;
  port: number;
  auto: boolean;
}

function parseArgs(argv: string[]): CliOptions {
  let host = "127.0.0.1";
  let port = 27777;
  let auto = false;
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--host") {
      host = argv[++i] ?? host;
    } else if (a === "--port") {
      port = Number(argv[++i] ?? port);
    } else if (a === "--auto") {
      auto = true;
    }
  }
  return { host, port, auto };
}

class ZeusUdpClient {
  private socket: dgram.Socket;
  private nextSeq = 1;
  connectionId = 0;
  sessionId = 0n;
  private lastAck = 0;
  private lastAckBits = 0;

  constructor(
    private readonly host: string,
    private readonly port: number,
  ) {
    this.socket = dgram.createSocket("udp4");
  }

  bindLocal(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.socket.once("error", reject);
      this.socket.bind(0, "0.0.0.0", () => {
        this.socket.removeListener("error", reject);
        resolve();
      });
    });
  }

  onError(handler: (err: Error) => void): void {
    this.socket.on("error", handler);
  }

  close(): void {
    try {
      this.socket.close();
    } catch {
      /* ignore */
    }
  }

  sendRaw(buf: Buffer): void {
    this.socket.send(buf, this.port, this.host);
  }

  waitOneMessage(timeoutMs: number): Promise<Buffer> {
    return new Promise((resolve, reject) => {
      const onMsg = (msg: Buffer) => {
        clearTimeout(t);
        resolve(msg);
      };
      const t = setTimeout(() => {
        this.socket.removeListener("message", onMsg);
        reject(new Error("recv timeout"));
      }, timeoutMs);
      this.socket.once("message", onMsg);
    });
  }

  private buildHeaderBase(
    opcode: number,
    channel: ENetChannel,
    delivery: ENetDelivery,
    connId: number,
  ): {
    opcode: number;
    channel: ENetChannel;
    delivery: ENetDelivery;
    sequence: number;
    ack: number;
    ackBits: number;
    connectionId: number;
    flags: number;
  } {
    const seq = this.nextSeq++;
    return {
      opcode,
      channel,
      delivery,
      sequence: seq,
      ack: this.lastAck,
      ackBits: this.lastAckBits,
      connectionId: connId >>> 0,
      flags: 0,
    };
  }

  updateAckFromServer(header: import("./ZeusPacketParser.js").ParsedHeader): void {
    this.lastAck = header.sequence >>> 0;
    this.lastAckBits = header.ackBits >>> 0;
  }

  sendConnectRequest(): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const w = builder.payloadWriter();
    w.writeUInt16(ZEUS_PROTOCOL_VERSION);
    w.writeStringUtf8("ClientTest/0.0.1");
    w.writeUInt64(BigInt(Date.now()));
    const header = this.buildHeaderBase(
      EOpcode.C_CONNECT_REQUEST,
      ENetChannel.Loading,
      ENetDelivery.ReliableOrdered,
      0,
    );
    return builder.finalize(header);
  }

  sendPing(): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const w = builder.payloadWriter();
    const t = BigInt(Date.now());
    w.writeUInt64(t);
    const header = this.buildHeaderBase(EOpcode.C_PING, ENetChannel.Gameplay, ENetDelivery.Unreliable, this.connectionId);
    return builder.finalize(header);
  }

  sendDisconnect(reason = 0): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const w = builder.payloadWriter();
    w.writeUInt16(reason & 0xffff);
    const header = this.buildHeaderBase(
      EOpcode.C_DISCONNECT,
      ENetChannel.Gameplay,
      ENetDelivery.Reliable,
      this.connectionId,
    );
    return builder.finalize(header);
  }

  parseServerMessage(msg: Buffer): void {
    const parsed = parsePacket(msg);
    this.updateAckFromServer(parsed.header);
    const op = parsed.header.opcode;
    const pr = new ZeusPacketReader(parsed.payload, parsed.payload.length);
    if (op === EOpcode.S_CONNECT_OK) {
      this.connectionId = pr.readUInt32() >>> 0;
      this.sessionId = pr.readUInt64();
      const serverTimeMs = pr.readUInt64();
      const heartbeat = pr.readUInt32();
      console.log(
        `[Client] Connected connectionId=${this.connectionId} sessionId=${this.sessionId.toString()} serverTimeMs=${serverTimeMs.toString()} heartbeatMs=${heartbeat}`,
      );
    } else if (op === EOpcode.S_CONNECT_REJECT) {
      const code = pr.readUInt16();
      const reason = pr.readStringUtf8();
      console.log(`[Client] REJECT reasonCode=${code} msg=${reason}`);
    } else if (op === EOpcode.S_PONG) {
      const clientTime = pr.readUInt64();
      const serverTime = pr.readUInt64();
      const now = BigInt(Date.now());
      const rtt = Number(now - clientTime);
      console.log(
        `[Client] S_PONG clientTimeMs=${clientTime.toString()} serverTimeMs=${serverTime.toString()} RTT~=${rtt}ms`,
      );
    } else if (op === EOpcode.S_DISCONNECT_OK) {
      const serverTime = pr.readUInt64();
      console.log(`[Client] S_DISCONNECT_OK serverTimeMs=${serverTime.toString()}`);
    } else {
      console.log(`[Client] Unhandled opcode ${op}`);
    }
  }
}

async function doConnect(client: ZeusUdpClient): Promise<void> {
  const buf = client.sendConnectRequest();
  console.log("[Client] Sending C_CONNECT_REQUEST");
  client.sendRaw(buf);
  const reply = await client.waitOneMessage(3000);
  client.parseServerMessage(reply);
}

async function doPing(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  console.log("[Client] Sending C_PING");
  client.sendRaw(client.sendPing());
  const reply = await client.waitOneMessage(3000);
  client.parseServerMessage(reply);
}

async function doDisconnect(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  console.log("[Client] Sending C_DISCONNECT");
  client.sendRaw(client.sendDisconnect(0));
  const reply = await client.waitOneMessage(3000);
  client.parseServerMessage(reply);
  client.connectionId = 0;
  client.sessionId = 0n;
  console.log("[Client] Disconnected (local state cleared)");
}

async function runAuto(client: ZeusUdpClient): Promise<void> {
  await doConnect(client);
  for (let i = 0; i < 3; i++) {
    await doPing(client);
  }
  await doDisconnect(client);
}

async function runInteractive(client: ZeusUdpClient): Promise<void> {
  const rl = readline.createInterface({ input, output });
  console.log("Comandos: connect | ping | disconnect | quit");
  for (;;) {
    const line = (await rl.question("> ")).trim().toLowerCase();
    if (line === "quit" || line === "exit") {
      break;
    }
    if (line === "connect") {
      await doConnect(client);
    } else if (line === "ping") {
      await doPing(client);
    } else if (line === "disconnect") {
      await doDisconnect(client);
    } else if (line) {
      console.log("Comando desconhecido.");
    }
  }
  rl.close();
}

async function main(): Promise<void> {
  const opts = parseArgs(process.argv);
  const client = new ZeusUdpClient(opts.host, opts.port);
  await client.bindLocal();
  client.onError((err) => {
    console.error("[Client] socket error", err);
  });

  try {
    if (opts.auto) {
      await runAuto(client);
    } else {
      await runInteractive(client);
    }
  } finally {
    client.close();
  }
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
