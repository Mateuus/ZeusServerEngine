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

### `ZEUS_PROTOCOL_VERSION`

- **2** (atual): handshake com challenge, filas de envio, ACK/reenvio de reliable, fragmentação de loading, simulador de perda opcional.

### Canais / entrega (controlos)

| Fluxo | Canal (`ENetChannel`) | Entrega (`ENetDelivery`) |
|--------|------------------------|---------------------------|
| `C_CONNECT_REQUEST` | `Loading` | `Reliable` ou `ReliableOrdered` |
| `S_CONNECT_CHALLENGE` / `S_CONNECT_OK` / `S_CONNECT_REJECT` | `Loading` | `ReliableOrdered` |
| `C_CONNECT_RESPONSE` | `Loading` | `ReliableOrdered` |
| `C_PING` / `S_PONG` | `Gameplay` | `Unreliable` (ou `UnreliableSequenced`) |
| `C_DISCONNECT` / `S_DISCONNECT_OK` | `Gameplay` | `Reliable` ou `ReliableOrdered` |
| `C_LOADING_FRAGMENT` / `S_LOADING_ASSEMBLED_OK` | `Loading` | `ReliableOrdered` |

### `flags` no cabeçalho

- Em mensagens **`ReliableOrdered`** (e `UnreliableSequenced` no servidor), o campo `flags` transporta um **order id por canal** (`NetConnection::NextReliableOrderId` / `SubmitInboundReliableOrdered`).
- **`ReliableOrdered` com reordenação UDP**: `NetConnection::SubmitInboundReliableOrdered` mantém um **mapa pendente por canal** (limite `MaxOrderedPendingPerChannel`, salto máximo `MaxOrderedGap`). Mensagens com `orderId` futuro são enfileiradas até chegar o `orderId` esperado; `orderId` antigo é tratado como duplicata. Canais usam contadores independentes (Gameplay não bloqueia Loading, etc.).

### Fila, ACK e reenvio

- Saídas passam por **`SendQueue`** por canal, drenadas no fim do tick com orçamento em bytes (`ChannelConfigs`).
- Pacotes **reliable** enviados ficam registados em **`ReliabilityLayer` / `PacketAckTracker`** até o cliente ACKar via `ack` / `ackBits` do cabeçalho; **reenvio** periódico com limite de tentativas.

## Handshake (protocolo 2)

1. Cliente envia `C_CONNECT_REQUEST` com `connectionId = 0` no cabeçalho.
2. Servidor cria `NetConnection` e `ClientSession` em estado **`Connecting`**, envia `S_CONNECT_CHALLENGE` (`serverNonce`, `connectionId` no payload).
3. Cliente responde `C_CONNECT_RESPONSE` (`clientNonce` do pedido + `serverNonce` do challenge), `ReliableOrdered` no canal `Loading`, `connectionId` no cabeçalho igual ao recebido no challenge.
4. Servidor valida nonces e ordenação `ReliableOrdered` no canal `Loading`, passa a sessão a **`Connected`**, envia `S_CONNECT_OK`.
5. **`C_CONNECT_RESPONSE` inválido** (payload ilegível ou nonces incoerentes com a sessão em `Connecting`): o servidor envia `S_CONNECT_REJECT` (`InvalidPacket` ou `InvalidHandshake`) e remove sessão + conexão.
6. **Idempotência**: se o mesmo `UdpEndpoint` já tiver sessão em `Connected`, um novo `C_CONNECT_REQUEST` válido recebe de novo o **mesmo** `S_CONNECT_OK` (útil com perda UDP).
7. Sessões em estado **`Connecting`** no mesmo endpoint: novo `C_CONNECT_REQUEST` válido **reenvia o mesmo challenge** (mesmo `serverNonce`), atualizando o `clientNonce` do pedido.

### Payload `C_CONNECT_REQUEST`

| Campo | Tipo |
|--------|------|
| `clientProtocolVersion` | `uint16` |
| `clientBuild` | `string` (UTF-8, prefixo `uint16` length) |
| `clientNonce` | `uint64` |

Validação: `clientProtocolVersion` deve ser `ZEUS_PROTOCOL_VERSION`; caso contrário `S_CONNECT_REJECT` com `reasonCode = 1` (InvalidProtocolVersion).  
`clientBuild` não pode ser vazio (após trim ASCII) nem exceder **256** bytes UTF-8; caso contrário `S_CONNECT_REJECT` (`InvalidPacket`).

### Payload `S_CONNECT_CHALLENGE`

| Campo | Tipo |
|--------|------|
| `serverNonce` | `uint64` |
| `connectionId` | `uint32` |

### Payload `C_CONNECT_RESPONSE`

| Campo | Tipo |
|--------|------|
| `clientNonce` | `uint64` |
| `serverNonce` | `uint64` |

### Payload `S_CONNECT_OK`

| Campo | Tipo |
|--------|------|
| `connectionId` | `uint32` |
| `sessionId` | `uint64` |
| `serverTimeMs` | `uint64` |
| `heartbeatIntervalMs` | `uint32` (fixo 5000 nesta fase) |

### Payload `C_LOADING_FRAGMENT` (teste / loading)

| Campo | Tipo |
|--------|------|
| `snapshotId` | `uint64` |
| `chunkIndex` | `uint16` |
| `chunkCount` | `uint16` |
| `dataLen` | `uint16` |
| `data` | `dataLen` bytes |

Quando todos os índices `0 .. chunkCount-1` foram recebidos, o servidor envia `S_LOADING_ASSEMBLED_OK` com `snapshotId` (`uint64`).

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
| 4 | InvalidHandshake (`C_CONNECT_RESPONSE` com nonces inválidos) |

## Ping / Pong

- Payload `C_PING`: `clientTimeMs` (`uint64`, epoch Unix ms recomendado pelo cliente).
- Payload `S_PONG`: `clientTimeMs` (`uint64`) + `serverTimeMs` (`uint64`).

**Regra:** `C_PING` só é aceite se existir `NetConnection` + `ClientSession` em `Connected`, `connectionId` do cabeçalho coincide com a conexão e o `UdpEndpoint` coincide com o da conexão. Caso contrário o servidor **ignora** o datagrama (sem resposta) — evita ruído com spoofing de ids. **Ping antes do handshake** ou com `connectionId` inválido: ignorado (sem `S_PONG`).

## Disconnect

1. Cliente envia `C_DISCONNECT` (`reasonCode: uint16` no payload).
2. Servidor responde `S_DISCONNECT_OK` (`serverTimeMs: uint64`).
3. Remove `ClientSession` e `NetConnection`.

## Identificadores

- **`ConnectionId`**: incremental no servidor (`1, 2, …`), transportado no cabeçalho após handshake.
- **`SessionId`**: incremental `uint64` no `SessionManager`, devolvido em `S_CONNECT_OK`.

## Sequence / Ack / AckBits

- `NetConnection::NextSequence()` gera o `sequence` de **saída** do servidor para essa conexão.
- `MarkReceived` / `GetAck` / `GetAckBits` implementam histórico básico de 32 bits relativamente ao último `sequence` remoto visto.
- Cada datagrama recebido do cliente chama `ApplyInboundAck(ack, ackBits)` para libertar envios reliable já confirmados pelo cliente.

## Timeout

- `NetConnectionManager::UpdateTimeouts` remove conexões sem tráfego recebido há **`ConnectionTimeoutMs`** (por defeito **30000** ms em `Config/server.json`; mínimo aplicado no arranque: **1000** ms). Sessões associadas são removidas no mesmo tick. Logs: `[Session] Timeout sessionId=... connectionId=...` e `[Net] Connection removed connectionId=... reason=timeout`.
- **Reliable esgotado**: se um pacote reliable exceder `ReliableMaxResends` reenvios, a entrada é descartada; se ocorrerem falhas deste tipo num tick, a conexão é removida (e a sessão associada). Logs throttled em `[Reliable]` (tentativas 1, 3, 5 e falha final).

## Configuração

`Config/server.json`:

- **`ListenUdpPort`**: `0` = sem UDP; `1`–`65535` = bind em `0.0.0.0:porta` e processamento no tick fixo.
- **`NetSimDropPermille`**: `0`–`1000` — probabilidade aproximada de **descartar** datagramas UDP recebidos (desenvolvimento); `0` = desligado. `100` ≈ 10%, `300` ≈ 30%, `700` ≈ 70%, `1000` = 100% (handshake torna-se impraticável).
- **`ConnectionTimeoutMs`**: inatividade (sem datagramas recebidos) antes de remover a conexão (por defeito `30000`).
- **`NetworkDebugAck`**: `true` para logs `[NetAck] rx sequence=... ack=... ackBits=...` em cada datagrama aceite (uso pontual; pode ser verboso).
- **`ReliableResendIntervalMs`**: intervalo mínimo entre reenvios de um pacote reliable (por defeito `250`).
- **`ReliableMaxResends`**: máximo de reenvios por sequência reliable antes de desistir (por defeito `12`; `0` desativa reenvios).
- **`MaxLoadingFragmentCount`**, **`MaxReassemblyBytes`**, **`ReassemblyTimeoutMs`**: limites ao reassembly de `C_LOADING_FRAGMENT` (por defeito `4096`, `4194304`, `60000`).
- **`MaxOrderedPendingPerChannel`**, **`MaxOrderedGap`**: fila de reordenação `ReliableOrdered` (por defeito `64` e `128`).

## Métricas e diagnóstico

- `PacketStats` (contadores: recebidos, rejeitados por regras/parse, drops do simulador, reenvios reliable, give-ups reliable, fila ordered cheia, envios, assemblies de loading concluídos).
- `NetworkDiagnostics::LastRttMs` atualizado a partir de `C_PING` (estimativa grosseira).

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

Comandos interativos (entre outros): `connect`, `ping`, `disconnect`, `reliable`, `ordered`, `ordered-ooo`, `fragment`, `spam-ping`, `reconnect`, `auto`, `stress-basic`, `quit`.

Modo automático mínimo (`connect` → `ping` → `reliable` → `ordered` → `disconnect`):

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --auto
```

Ou comando posicional:

```bash
node dist/index.js --host 127.0.0.1 --port 27777 auto
```

Opcodes de teste (sem gameplay): `C_TEST_RELIABLE` / `S_TEST_RELIABLE` (1100/1101), `C_TEST_ORDERED` / `S_TEST_ORDERED` (1110/1111), canal `Gameplay`.

## Ainda **não** implementado

Ver [NEXT_STEPS.md](NEXT_STEPS.md): gameplay, movimento, replicação, AOI, Jolt, etc.
