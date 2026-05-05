# Zeus Server Engine — testes de rede (Parte 2.5)

Guia para validar handshake, ping, disconnect, ACK, reliable, `ReliableOrdered`, fragmentação, timeout e simulador de perda **sem** gameplay.

## Compilar o servidor

- **Windows (Visual Studio):** abrir `ZeusServerEngine.sln`, configuração **Debug** ou **Release**, plataforma **x64**, **Build Solution**.
- Saída: `bin/Debug/ZeusServer.exe` (ou `bin/Release/`).

## Correr o servidor

A partir da pasta de saída (para `Config/server.json` relativo funcionar):

```bat
cd f:\ZeusProject\Server\ZeusServerEngine\bin\Debug
ZeusServer.exe
```

Ou com caminho absoluto ao JSON:

```bat
ZeusServer.exe f:\ZeusProject\Server\ZeusServerEngine\Config\server.json
```

## `ListenUdpPort`

Em `Config/server.json`:

- `0` — UDP desligado.
- `1`–`65535` — servidor escuta em `0.0.0.0:<porta>`.

## `NetSimDropPermille`

Em `Config/server.json`, valor `0`–`1000` (permilagem aproximada de descarte na **receção**):

| Valor | Efeito aproximado |
|--------|-------------------|
| 0 | Sem perda |
| 100 | ~10% |
| 300 | ~30% |
| 700 | ~70% |
| 1000 | 100% (handshake quase impossível) |

Reiniciar o servidor após alterar o JSON.

## Outras chaves úteis (Parte 2.5)

- `ConnectionTimeoutMs` — inatividade antes de remover conexão (ex.: `3000` para testes rápidos).
- `NetworkDebugAck` — `true` para logs `[NetAck]`.
- `ReliableResendIntervalMs`, `ReliableMaxResends` — comportamento de reenvio.
- `MaxLoadingFragmentCount`, `MaxReassemblyBytes`, `ReassemblyTimeoutMs` — limites de reassembly.
- `MaxOrderedPendingPerChannel`, `MaxOrderedGap` — fila `ReliableOrdered`.

## ClientTest

```bash
cd f:\ZeusProject\Server\ZeusServerEngine\ClientTest
npm install
npm run build
```

Modo automático mínimo:

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --auto
```

Comando posicional equivalente:

```bash
node dist/index.js --host 127.0.0.1 --port 27777 auto
```

Comandos interativos: ver mensagem de ajuda ao iniciar sem argumentos.

Sequência em ficheiro (recomendado no Windows em vez de pipe para stdin):

```bash
node dist/index.js --host 127.0.0.1 --port 27777 --commands-file scripts/network-validate-006-009-011.cmds
```

Uma linha por comando; linhas que começam por `#` são comentários; `wait 4000` espera 4000 ms (útil para TEST-010). Ficheiros de exemplo em `ClientTest/scripts/`.

## Cenários (resultado esperado)

| ID | Cenário | Passos | Esperado |
|----|---------|--------|----------|
| TEST-001 | Handshake válido | `auto` ou `connect` | `S_CONNECT_OK`, `connectionId` e `sessionId` > 0 |
| TEST-002 | Handshake versão inválida | Cliente com `ZEUS_PROTOCOL_VERSION` errado (editar ClientTest) | `S_CONNECT_REJECT` código 1 |
| TEST-003 | Ping/Pong após connect | `connect` → `ping` | `S_PONG`, RTT no log do cliente |
| TEST-004 | Ping antes do connect | `ping` sem `connect` | Sem resposta; cliente “Not connected” ou timeout |
| TEST-005 | Disconnect | `connect` → `disconnect` | `S_DISCONNECT_OK`, estado local limpo |
| TEST-006 | Reconnect | `disconnect` → `connect` | Novo handshake OK |
| TEST-007 | Reliable sem perda | `NetSimDropPermille=0`, `reliable` após connect | `S_TEST_RELIABLE` recebido |
| TEST-008 | Reliable com perda | `NetSimDropPermille=300`, `reliable` | Eventual sucesso; logs `[Reliable]` no servidor |
| TEST-009 | ReliableOrdered fora de ordem | `ordered-ooo` após connect | Dois `S_TEST_ORDERED` após enviar ordem 1 depois 0 |
| TEST-010 | Timeout | `ConnectionTimeoutMs=3000`, cliente parado | Após ~3 s logs timeout; `ping` falha até novo `connect` |
| TEST-011 | Fragmentação Loading | `fragment` após connect | `S_LOADING_ASSEMBLED_OK` |
| TEST-012 | Canal inválido | Enviar opcode com canal/delivery errados (ferramenta à parte) | `DatagramsRejectedOpcodeRules` sobe; sem crash |

## Limitações conhecidas

- Simulador: apenas **perda** (`NetSimDropPermille`); latência, jitter, duplicados e reordenação no simulador estão para fases futuras (ver `Docs/NEXT_STEPS.md`).
- `ReliableOrdered` usa ordem **por canal**; saltos de `orderId` acima de `MaxOrderedGap` devolvem fila cheia e o pacote é descartado.
- Reassembly aborta e limpa estado se exceder `MaxReassemblyBytes` ou `ReassemblyTimeoutMs`.

## Parte 3

Só avançar para **World mínimo** após fechar o checklist da Parte 2.5 (build, `--auto`, documentação e ausência de dependências de gameplay na rede).
