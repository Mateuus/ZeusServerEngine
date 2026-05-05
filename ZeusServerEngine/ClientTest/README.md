# ClientTest — cliente UDP mínimo (Parte 2)

Cliente Node.js/TypeScript para validar **handshake**, **ping/pong** e **disconnect** contra `ZeusServer` com `ListenUdpPort` > 0.

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

Comandos no prompt: `connect`, `ping`, `disconnect`, `quit`.

Modo automático:

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --auto
```

## Ficheiros

| Ficheiro | Função |
|----------|--------|
| `src/ZeusProtocol.ts` | Magic, versão, limites, enums |
| `src/ZeusPacketWriter.ts` / `ZeusPacketReader.ts` | LE binário |
| `src/ZeusPacketBuilder.ts` | Cabeçalho + payload |
| `src/ZeusPacketParser.ts` | Validação de datagrama recebido |
| `src/index.ts` | CLI e fluxo |
