# Zeus Server Engine — rede (Parte 2)

Este documento descreve a camada **ZeusProtocol** + **ZeusNet** + **ZeusSession** no rebuild limpo. Ver também `.plans/03_ZEUS_NETWORK_CLEAN_PART2.md`.

## Princípios

- **Sem gameplay**: transporte, parsing, sessão e controlos de ligação apenas.
- **Little-endian** em todos os campos numéricos do wire nesta fase.
- **UDP** com datagramas até **1200 bytes** (`ZEUS_MAX_PACKET_BYTES`).
- **Magic** único: fourCC `Z` `E` `U` `S` no wire (primeiro byte do `uint32` little-endian = `'Z'`). O valor numérico é construído em código como  
  `(uint32_t)'Z' | ('E'<<8) | ('U'<<16) | ('S'<<24)` (C++) e o mesmo padrão no **ClientTest** (`magicToUInt32()`). Não misturar com outra convenção de bytes.

## Tempos

- **Monotónico** (`Zeus::Platform::NowMonotonicSeconds()`): timeouts de conexão, `createdAt` / `lastPacketAt` em sessão e `NetConnection`.
- **Epoch Unix em milissegundos** (`Zeus::Platform::NowUnixEpochMilliseconds()`): campos `serverTimeMs` / `clientTimeMs` nos payloads de controlo.

## Cabeçalho (32 bytes)

| Offset | Tipo     | Campo         |
|--------|----------|---------------|
| 0      | `uint32` | magic         |
| 4      | `uint16` | version       |
| 6      | `uint16` | headerSize    |
| 8      | `uint16` | opcode        |
| 10     | `uint8`  | channel       |
| 11     | `uint8`  | delivery      |
| 12     | `uint32` | sequence      |
| 16     | `uint32` | ack           |
| 20     | `uint32` | ackBits       |
| 24     | `uint32` | connectionId  |
| 28     | `uint16` | payloadSize   |
| 30     | `uint16` | flags         |

O `PacketParser` valida magic, versão, `headerSize == 32`, tamanhos e limite de payload.

### Canais / entrega sugeridos (controlos)

| Fluxo | Canal (`ENetChannel`) | Entrega (`ENetDelivery`) |
|--------|------------------------|---------------------------|
| `C_CONNECT_REQUEST` / `S_CONNECT_OK` / `S_CONNECT_REJECT` | `Loading` | `ReliableOrdered` |
| `C_PING` / `S_PONG` | `Gameplay` | `Unreliable` |
| `C_DISCONNECT` / `S_DISCONNECT_OK` | `Gameplay` | `Reliable` |

*Nota:* `Reliable` / `ReliableOrdered` nos headers são **semântica preparada**; ainda **não** há fila de reenvio nem ordenação garantida na camada de transporte.

## Handshake

1. Cliente envia `C_CONNECT_REQUEST` com `connectionId = 0` no cabeçalho (primeiro pedido).
2. Servidor cria `NetConnection` (id incremental), `ClientSession` (`sessionId` incremental), responde `S_CONNECT_OK` com ids e tempos.
3. **Idempotência**: se o mesmo `UdpEndpoint` já tiver sessão em `Connected`, um novo `C_CONNECT_REQUEST` válido recebe de novo o **mesmo** `S_CONNECT_OK` (útil com perda UDP).
4. Sessões em estado não final **Connecting** no mesmo endpoint são removidas antes de um novo handshake.

### Payload `C_CONNECT_REQUEST`

| Campo | Tipo |
|--------|------|
| `clientProtocolVersion` | `uint16` |
| `clientBuild` | `string` (UTF-8, prefixo `uint16` length) |
| `clientNonce` | `uint64` |

Validação: `clientProtocolVersion` deve ser `ZEUS_PROTOCOL_VERSION`; caso contrário `S_CONNECT_REJECT` com `reasonCode = 1` (InvalidProtocolVersion).

### Payload `S_CONNECT_OK`

| Campo | Tipo |
|--------|------|
| `connectionId` | `uint32` |
| `sessionId` | `uint64` |
| `serverTimeMs` | `uint64` |
| `heartbeatIntervalMs` | `uint32` (fixo 5000 nesta fase) |

### Payload `S_CONNECT_REJECT`

| Campo | Tipo |
|--------|------|
| `reasonCode` | `uint16` |
| `reasonMessage` | `string` |

Motivos iniciais (`ConnectRejectReason`):

| Código | Significado |
|--------|-------------|
| 1 | InvalidProtocolVersion |
| 2 | ServerFull |
| 3 | InvalidPacket |

## Ping / Pong

- Payload `C_PING`: `clientTimeMs` (`uint64`, epoch Unix ms recomendado pelo cliente).
- Payload `S_PONG`: `clientTimeMs` (`uint64`) + `serverTimeMs` (`uint64`).

**Regra:** `C_PING` só é aceite se existir `NetConnection` + `ClientSession` em `Connected`, `connectionId` do cabeçalho coincide com a conexão e o `UdpEndpoint` coincide com o da conexão. Caso contrário o servidor **ignora** o datagrama (sem resposta) — evita ruído com spoofing de ids.

## Disconnect

1. Cliente envia `C_DISCONNECT` (`reasonCode: uint16` no payload).
2. Servidor responde `S_DISCONNECT_OK` (`serverTimeMs: uint64`).
3. Remove `ClientSession` e `NetConnection`.

## Identificadores

- **`ConnectionId`**: incremental no servidor (`1, 2, …`), transportado no cabeçalho após handshake.
- **`SessionId`**: incremental `uint64` no `SessionManager`, devolvido em `S_CONNECT_OK`.

## Sequence / Ack / AckBits (básico)

- `NetConnection::NextSequence()` gera o `sequence` de **saída** do servidor para essa conexão.
- `MarkReceived` / `GetAck` / `GetAckBits` implementam histórico básico de 32 bits relativamente ao último `sequence` remoto visto (sem reenvio).

## Timeout

- `NetConnectionManager::UpdateTimeouts` remove conexões sem tráfego recebido há **30 s** (constante; configurável no futuro). Sessões associadas são removidas no mesmo tick.

## Configuração

`Config/server.json`:

- **`ListenUdpPort`**: `0` = sem UDP; `1`–`65535` = bind em `0.0.0.0:porta` e processamento no tick fixo.

Log esperado à subida:

```txt
[Net] UDP server listening on 0.0.0.0:27777
```

## ClientTest (`ClientTest/`)

Requisitos: Node.js + `npm install` na pasta `ClientTest`.

```bash
cd ClientTest
npm install
npm run build
npm start -- --host 127.0.0.1 --port 27777
```

Comandos interativos: `connect`, `ping`, `disconnect`, `quit`.

Modo automático (handshake + 3× ping + disconnect):

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --auto
```

## Ainda **não** implementado

Ver [NEXT_STEPS.md](NEXT_STEPS.md): reenvio reliable completo, `ReliableOrdered` real, timeout avançado, gameplay, movimento, replicação, AOI, Jolt, etc.
