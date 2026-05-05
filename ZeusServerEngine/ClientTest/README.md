# ClientTest — cliente UDP mínimo (Parte 2 + 2.5)

Cliente Node.js/TypeScript para validar **handshake v2**, **ping/pong**, **disconnect**, **reliable**, **`ReliableOrdered`**, **fragmentação Loading** e fluxos básicos contra `ZeusServer` com `ListenUdpPort` > 0.

## Pré-requisitos

- Node.js 18+ (recomendado 20+)
- Servidor compilado com UDP ativo, por exemplo `ListenUdpPort: 27777` em `Config/server.json`

## Instalação e build

```bash
cd ClientTest
npm install
npm run build
```

## Execução

```bash
npm start -- --host 127.0.0.1 --port 27777
```

Comandos no prompt: `connect`, `ping`, `disconnect`, `reliable`, `ordered`, `ordered-ooo`, `fragment`, `spam-ping`, `reconnect`, `auto`, `stress-basic`, `quit`.

Modo automático mínimo (connect → ping → reliable → ordered → disconnect):

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --auto
```

Stress básico (vários ciclos connect/ping/disconnect):

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --stress-basic
```

## Ficheiros

| Ficheiro | Função |
|----------|--------|
| `src/ZeusProtocol.ts` | Magic, versão, limites, enums, opcodes |
| `src/ZeusPacketWriter.ts` / `ZeusPacketReader.ts` | LE binário |
| `src/ZeusPacketBuilder.ts` | Cabeçalho + payload |
| `src/ZeusPacketParser.ts` | Validação de datagrama recebido |
| `src/index.ts` | CLI e fluxos de teste |

Cenários documentados no servidor: [`../Docs/NETWORK_TESTS.md`](../Docs/NETWORK_TESTS.md).
