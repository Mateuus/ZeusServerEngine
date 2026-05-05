import * as dgram from "node:dgram";
import * as fs from "node:fs/promises";
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
import type { ParsedHeader } from "./ZeusPacketParser.js";

interface CliOptions {
  host: string;
  port: number;
  auto: boolean;
  stressBasic: boolean;
  /** Ficheiro com um comando por linha (`wait <ms>` suportado). */
  commandsFile: string | null;
  /** Primeiro argumento posicional (ex.: `auto`, `connect`). */
  command: string | null;
}

function parseArgs(argv: string[]): CliOptions {
  let host = "127.0.0.1";
  let port = 27777;
  let auto = false;
  let stressBasic = false;
  let commandsFile: string | null = null;
  let command: string | null = null;
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--host") {
      host = argv[++i] ?? host;
    } else if (a === "--port") {
      port = Number(argv[++i] ?? port);
    } else if (a === "--auto") {
      auto = true;
    } else if (a === "--stress-basic") {
      stressBasic = true;
    } else if (a === "--commands-file") {
      commandsFile = argv[++i] ?? null;
    } else if (a.startsWith("--")) {
      /* skip unknown flags */
    } else if (command === null) {
      command = a;
    }
  }
  return { host, port, auto, stressBasic, commandsFile, command };
}

class ZeusUdpClient {
  private socket: dgram.Socket;
  private nextSeq = 1;
  connectionId = 0;
  sessionId = 0n;
  private lastAck = 0;
  private lastAckBits = 0;
  private connectClientNonce = 0n;
  private pendingServerChallenge = 0n;
  /** `ReliableOrdered` / sequenciamento por canal (espelha `NetConnection::NextReliableOrderId`). */
  private orderIdByChannel: number[] = [0, 0, 0, 0, 0];

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

  /** Novo handshake na mesma socket: servidor espera `orderId` 0 em ReliableOrdered. */
  resetReliableOrderState(): void {
    this.orderIdByChannel = [0, 0, 0, 0, 0];
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

  private nextOrderFlags(channel: ENetChannel): number {
    const ch = channel as number;
    const v = this.orderIdByChannel[ch]! & 0xffff;
    this.orderIdByChannel[ch] = (this.orderIdByChannel[ch]! + 1) & 0xffff;
    return v;
  }

  private buildHeaderBase(
    opcode: number,
    channel: ENetChannel,
    delivery: ENetDelivery,
    connId: number,
    flagsOverride?: number,
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
    const useOrdered =
      delivery === ENetDelivery.ReliableOrdered || delivery === ENetDelivery.UnreliableSequenced;
    const flags =
      flagsOverride !== undefined ? flagsOverride & 0xffff : useOrdered ? this.nextOrderFlags(channel) : 0;
    return {
      opcode,
      channel,
      delivery,
      sequence: seq,
      ack: this.lastAck,
      ackBits: this.lastAckBits,
      connectionId: connId >>> 0,
      flags,
    };
  }

  updateAckFromServer(header: ParsedHeader): void {
    this.lastAck = header.sequence >>> 0;
    this.lastAckBits = header.ackBits >>> 0;
  }

  sendConnectRequest(): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const w = builder.payloadWriter();
    w.writeUInt16(ZEUS_PROTOCOL_VERSION);
    w.writeStringUtf8("ClientTest/0.0.1");
    this.connectClientNonce = BigInt(Date.now());
    w.writeUInt64(this.connectClientNonce);
    const header = this.buildHeaderBase(
      EOpcode.C_CONNECT_REQUEST,
      ENetChannel.Loading,
      ENetDelivery.Reliable,
      0,
    );
    return builder.finalize(header);
  }

  sendConnectResponse(): Buffer {
    /** Cada envio físico de `C_CONNECT_RESPONSE` deve usar `orderId` 0 (retransmissão após perda). */
    this.orderIdByChannel[ENetChannel.Loading] = 0;
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const w = builder.payloadWriter();
    w.writeUInt64(this.connectClientNonce);
    w.writeUInt64(this.pendingServerChallenge);
    const header = this.buildHeaderBase(
      EOpcode.C_CONNECT_RESPONSE,
      ENetChannel.Loading,
      ENetDelivery.ReliableOrdered,
      this.connectionId,
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

  sendTestReliable(): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const header = this.buildHeaderBase(
      EOpcode.C_TEST_RELIABLE,
      ENetChannel.Gameplay,
      ENetDelivery.Reliable,
      this.connectionId,
    );
    return builder.finalize(header);
  }

  sendTestOrderedManual(orderId: number): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const seq = this.nextSeq++;
    const h = {
      opcode: EOpcode.C_TEST_ORDERED,
      channel: ENetChannel.Gameplay,
      delivery: ENetDelivery.ReliableOrdered,
      sequence: seq,
      ack: this.lastAck,
      ackBits: this.lastAckBits,
      connectionId: this.connectionId >>> 0,
      flags: orderId & 0xffff,
    };
    return builder.finalize(h);
  }

  sendTestOrderedSequential(): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const header = this.buildHeaderBase(
      EOpcode.C_TEST_ORDERED,
      ENetChannel.Gameplay,
      ENetDelivery.ReliableOrdered,
      this.connectionId,
    );
    return builder.finalize(header);
  }

  sendLoadingFragment(
    snapshotId: bigint,
    chunkIndex: number,
    chunkCount: number,
    data: Buffer,
    orderId: number,
  ): Buffer {
    const builder = new ZeusPacketBuilder();
    builder.reset();
    const w = builder.payloadWriter();
    w.writeUInt64(snapshotId);
    w.writeUInt16(chunkIndex & 0xffff);
    w.writeUInt16(chunkCount & 0xffff);
    w.writeUInt16(data.length & 0xffff);
    w.writeBytes(data);
    const seq = this.nextSeq++;
    const h = {
      opcode: EOpcode.C_LOADING_FRAGMENT,
      channel: ENetChannel.Loading,
      delivery: ENetDelivery.ReliableOrdered,
      sequence: seq,
      ack: this.lastAck,
      ackBits: this.lastAckBits,
      connectionId: this.connectionId >>> 0,
      flags: orderId & 0xffff,
    };
    return builder.finalize(h);
  }

  parseServerMessage(msg: Buffer): number {
    const parsed = parsePacket(msg);
    this.updateAckFromServer(parsed.header);
    const hdr = parsed.header;
    const op = hdr.opcode;
    const pr = new ZeusPacketReader(parsed.payload, parsed.payload.length);
    if (op === EOpcode.S_CONNECT_CHALLENGE) {
      this.pendingServerChallenge = pr.readUInt64();
      this.connectionId = pr.readUInt32() >>> 0;
      console.log(
        `[Client] S_CONNECT_CHALLENGE serverNonce=${this.pendingServerChallenge.toString()} connectionId=${this.connectionId}`,
      );
    } else if (op === EOpcode.S_CONNECT_OK) {
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
        `[Client] Ping RTT=${rtt}ms seq=${hdr.sequence} ack=${hdr.ack} ackBits=0x${hdr.ackBits.toString(16)} clientTimeMs=${clientTime.toString()} serverTimeMs=${serverTime.toString()}`,
      );
    } else if (op === EOpcode.S_DISCONNECT_OK) {
      const serverTime = pr.readUInt64();
      console.log(`[Client] S_DISCONNECT_OK serverTimeMs=${serverTime.toString()}`);
    } else if (op === EOpcode.S_LOADING_ASSEMBLED_OK) {
      const sid = pr.readUInt64();
      console.log(`[Client] S_LOADING_ASSEMBLED_OK snapshotId=${sid.toString()}`);
    } else if (op === EOpcode.S_TEST_RELIABLE) {
      console.log(`[Client] Reliable acked (S_TEST_RELIABLE) serverSeq=${hdr.sequence} ack=${hdr.ack}`);
    } else if (op === EOpcode.S_TEST_ORDERED) {
      console.log(`[Client] Ordered response (S_TEST_ORDERED) serverSeq=${hdr.sequence} flagsOrder=${hdr.flags}`);
    } else {
      console.log(`[Client] Unhandled opcode ${op}`);
    }
    return op;
  }
}

async function doConnect(client: ZeusUdpClient): Promise<void> {
  client.resetReliableOrderState();
  const buf = client.sendConnectRequest();
  console.log("[Client] Sending C_CONNECT_REQUEST");
  client.sendRaw(buf);
  const reply1 = await client.waitOneMessage(3000);
  client.parseServerMessage(reply1);
  const h1 = parsePacket(reply1).header;
  if (h1.opcode === EOpcode.S_CONNECT_CHALLENGE) {
    console.log("[Client] Sending C_CONNECT_RESPONSE");
    client.sendRaw(client.sendConnectResponse());
    const deadline = Date.now() + 30000;
    for (;;) {
      const waitMs = Math.min(5000, Math.max(1, deadline - Date.now()));
      if (waitMs <= 0) {
        throw new Error("Handshake timeout waiting for S_CONNECT_OK");
      }
      let reply2: Buffer;
      try {
        reply2 = await client.waitOneMessage(waitMs);
      } catch {
        console.log("[Client] Re-sending C_CONNECT_RESPONSE (recv timeout / loss)");
        client.sendRaw(client.sendConnectResponse());
        continue;
      }
      const op2 = client.parseServerMessage(reply2);
      if (op2 === EOpcode.S_CONNECT_OK) {
        break;
      }
      if (op2 === EOpcode.S_CONNECT_REJECT) {
        throw new Error("Handshake rejected after C_CONNECT_RESPONSE");
      }
      if (op2 === EOpcode.S_CONNECT_CHALLENGE) {
        console.log("[Client] Re-sending C_CONNECT_RESPONSE (challenge repeat / loss)");
        client.sendRaw(client.sendConnectResponse());
        continue;
      }
      throw new Error(`Expected S_CONNECT_OK after handshake, got opcode=${op2}`);
    }
  } else if (h1.opcode === EOpcode.S_CONNECT_OK) {
    /* idempotent: servidor reemitiu OK sem novo challenge */
  } else {
    throw new Error(`Unexpected first reply opcode=${h1.opcode}`);
  }
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
  console.log("[Client] Disconnected");
}

async function doReliable(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  console.log("[Client] Sending C_TEST_RELIABLE");
  client.sendRaw(client.sendTestReliable());
  const reply = await client.waitOneMessage(5000);
  client.parseServerMessage(reply);
}

async function doOrderedSequential(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  for (let i = 0; i < 3; i++) {
    console.log(`[Client] Sending C_TEST_ORDERED seq ${i + 1}/3`);
    client.sendRaw(client.sendTestOrderedSequential());
    const reply = await client.waitOneMessage(5000);
    client.parseServerMessage(reply);
  }
}

async function doOrderedOutOfOrder(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  console.log("[Client] Sending C_TEST_ORDERED out-of-order (order 1 then 0)");
  client.sendRaw(client.sendTestOrderedManual(1));
  client.sendRaw(client.sendTestOrderedManual(0));
  const r1 = await client.waitOneMessage(5000);
  client.parseServerMessage(r1);
  const r2 = await client.waitOneMessage(5000);
  client.parseServerMessage(r2);
}

async function doFragment(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  const snap = 424242n;
  const p0 = Buffer.from("aaa");
  const p1 = Buffer.from("bbb");
  const p2 = Buffer.from("ccc");
  console.log("[Client] Sending loading fragments (order 1,0,2)");
  client.sendRaw(client.sendLoadingFragment(snap, 1, 3, p1, 1));
  client.sendRaw(client.sendLoadingFragment(snap, 0, 3, p0, 0));
  client.sendRaw(client.sendLoadingFragment(snap, 2, 3, p2, 2));
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const msg = await client.waitOneMessage(Math.max(1, Math.min(2000, deadline - Date.now())));
    const op = client.parseServerMessage(msg);
    if (op === EOpcode.S_LOADING_ASSEMBLED_OK) {
      return;
    }
  }
  throw new Error("S_LOADING_ASSEMBLED_OK not received");
}

async function doSpamPing(client: ZeusUdpClient): Promise<void> {
  if (client.connectionId === 0) {
    console.log("[Client] Not connected.");
    return;
  }
  for (let i = 0; i < 20; i++) {
    client.sendRaw(client.sendPing());
  }
  for (let i = 0; i < 20; i++) {
    const reply = await client.waitOneMessage(3000);
    client.parseServerMessage(reply);
  }
}

async function doStressBasic(client: ZeusUdpClient): Promise<void> {
  for (let c = 0; c < 5; c++) {
    await doConnect(client);
    for (let p = 0; p < 10; p++) {
      await doPing(client);
    }
    await doDisconnect(client);
  }
}

async function runAuto(client: ZeusUdpClient): Promise<void> {
  await doConnect(client);
  await doPing(client);
  await doReliable(client);
  await doOrderedSequential(client);
  await doDisconnect(client);
}

/** Uma linha de script (`wait <ms>` ou comando `runOneCommand`). */
async function runScriptLine(raw: string, client: ZeusUdpClient): Promise<void> {
  const trimmed = raw.trim().toLowerCase();
  if (!trimmed || trimmed === "quit" || trimmed === "exit") {
    return;
  }
  const waitMatch = /^wait\s+(\d+)\s*$/.exec(trimmed);
  if (waitMatch) {
    const ms = Number(waitMatch[1]);
    if (Number.isFinite(ms) && ms >= 0 && ms <= 120000) {
      await new Promise<void>((resolve) => setTimeout(resolve, ms));
    } else {
      console.error("[Client] wait: use ms between 0 and 120000");
    }
    return;
  }
  await runOneCommand(trimmed, client);
}

async function runCommandsFromFile(client: ZeusUdpClient, filePath: string): Promise<void> {
  const text = await fs.readFile(filePath, "utf-8");
  const lines = text.split(/\r?\n/);
  for (const raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) {
      continue;
    }
    const low = line.toLowerCase();
    if (low === "quit" || low === "exit") {
      break;
    }
    await runScriptLine(line, client);
  }
}

async function runOneCommand(cmd: string, client: ZeusUdpClient): Promise<void> {
  const c = cmd.trim().toLowerCase();
  switch (c) {
    case "connect":
      await doConnect(client);
      return;
    case "ping":
      await doPing(client);
      return;
    case "disconnect":
      await doDisconnect(client);
      return;
    case "reliable":
      await doReliable(client);
      return;
    case "ordered":
      await doOrderedSequential(client);
      return;
    case "ordered-ooo":
      await doOrderedOutOfOrder(client);
      return;
    case "fragment":
      await doFragment(client);
      return;
    case "spam-ping":
      await doSpamPing(client);
      return;
    case "reconnect":
      await doDisconnect(client);
      await doConnect(client);
      return;
    case "auto":
      await runAuto(client);
      return;
    case "stress-basic":
      await doStressBasic(client);
      return;
    default:
      throw new Error(`Unknown command: ${cmd}`);
  }
}

async function runInteractive(client: ZeusUdpClient): Promise<void> {
  const rl = readline.createInterface({ input, output });
  console.log(
    "Comandos: connect | ping | disconnect | reliable | ordered | ordered-ooo | fragment | spam-ping | reconnect | auto | stress-basic | wait <ms> | quit",
  );
  for (;;) {
    const line = (await rl.question("> ")).trim().toLowerCase();
    if (line === "quit" || line === "exit") {
      break;
    }
    if (!line) {
      continue;
    }
    try {
      await runScriptLine(line, client);
    } catch (e) {
      console.error("[Client]", e);
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
    if (opts.commandsFile) {
      await runCommandsFromFile(client, opts.commandsFile);
    } else if (opts.auto || opts.command === "auto") {
      await runAuto(client);
    } else if (opts.stressBasic) {
      await doStressBasic(client);
    } else if (opts.command) {
      await runOneCommand(opts.command, client);
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
