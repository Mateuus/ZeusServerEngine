# Zeus Server Engine (Parte 1 + início Parte 2)

No mono-repo **ZeusProject**, este código vive em **`Server/ZeusServerEngine/`**. O repositório remoto `ZeusServerEngine` usa **`Server/` como raiz do Git** (ver [`../README.md`](../README.md)).

## Requisitos

### Windows

- Windows 10 ou superior (x64)
- Visual Studio 2022 com workload **Desktop development with C++** (MSVC v143, Windows 10 SDK)

### Linux

- GCC ou Clang com **C++20** (testado com CMake 3.20+)
- `pthread` e libc habituais (sem dependências externas além do compilador)

## Compilar

### Windows (Visual Studio)

1. Abrir `ZeusServerEngine.sln`
2. Selecionar configuração **Debug** ou **Release** e plataforma **x64**
3. **Build > Build Solution**

Saída: `bin/Debug/` ou `bin/Release/` — executável `ZeusServer.exe`. O pós-build copia `Config/*` para junto do `.exe`; **`Data/` continua na raiz do motor** (`ZeusServerEngine/Data`). O arranque resolve a raiz de conteúdo subindo a partir do executável até encontrar `Config/server.json` e **prioriza** a pasta que também tem `Data/` (tipicamente `Server/ZeusServerEngine/`).

### Windows / Linux (CMake)

```bash
cmake -S . -B build
cmake --build build --config Debug    # ou Release (multi-config MSVC)
```

Executável: `build/Debug/ZeusServer.exe` ou `build/Release/ZeusServer.exe` (MSVC), ou `build/ZeusServer` (single-config). **Não é obrigatório** copiar `Config`/`Data` para `build/` — os caminhos relativos no JSON (`Data/Maps/...`, `Logs`) resolvem-se contra a raiz detectada (pai do `Config` que contém `Data`).

## Executar

Podes correr o `.exe` a partir de `build/Debug` ou `bin/Debug`; o servidor descobre a raiz automaticamente. Opcional: define **`ZEUS_SERVER_ROOT`** com o caminho absoluto da pasta do motor (onde estão `Config` e `Data`).

```bat
build\Debug\ZeusServer.exe
```

Ou passar o caminho absoluto do JSON (a raiz de `Data`/`Logs` relativa passa a ser o pai de `Config`):

```bat
ZeusServer.exe C:\caminho\completo\para\Config\server.json
```

Parar: **Ctrl+C** (ou fechar a consola). No Linux, **SIGINT** e **SIGTERM** pedem paragem ao loop de forma compatível com handlers POSIX.

## Parte 1 — incluído

- `ZeusCore`, `ZeusPlatform`, `ZeusRuntime`, `ZeusApp` (`CoreServerApp`, `main.cpp`)
- Tick fixo (~30 TPS por defeito), anti-spiral; **TPS e contadores na barra de título da consola** (estilo servidores clássicos), não em spam no log
- Leitura mínima de `Config/server.json` sem bibliotecas JSON externas

### Config (`Config/server.json`)

| Chave | Descrição |
|--------|-----------|
| `TargetTPS` | Tick fixo alvo do `EngineLoop` |
| `LogLevel` | `Trace` … `Error` |
| `SessionLogDir` | Pasta onde é criado um **`.log` por arranque** (`ZeusServer_YYYYMMDD_HHMMSS.log`), cópia espelhada de tudo o que vai para o log da consola |
| `DebugOverlayRows` | Linhas fixas no **fundo do terminal** (VT + margem de scroll). `0` = desligado. Ideal no futuro para posição de player / replicação sem encher o stderr |
| `ListenUdpPort` | Se `1`–`65535`, abre **UDP** e valida pacotes Zeus em cada tick; `0` = sem rede (defeito) |
| `NetSimDropPermille` | `0`–`1000`: probabilidade aproximada de descartar datagramas recebidos (dev) |
| `ConnectionTimeoutMs` | Inatividade antes de remover a conexão (ms) |
| `NetworkDebugAck` | `true` para logs `[NetAck]` por datagrama |
| `ReliableResendIntervalMs` / `ReliableMaxResends` | Política de reenvio reliable |
| `MaxLoadingFragmentCount` / `MaxReassemblyBytes` / `ReassemblyTimeoutMs` | Limites de reassembly Loading |
| `MaxOrderedPendingPerChannel` / `MaxOrderedGap` | Fila `ReliableOrdered` por canal |

## Parte 2 + 2.5 — rede (handshake, ping, reliable, ordered, fragmentos, timeout)

- **`ZeusProtocol`**: wire + `SessionWire` (payloads connect/reject/ping/pong/disconnect, fragmentos).
- **`ZeusNet`**: `UdpSocket`, `UdpServer`, `UdpEndpoint`, `NetConnection`, `NetConnectionManager`, `SendQueue`, `ReliabilityLayer`, `PacketAckTracker`, `NetworkSimulator`.
- **`ZeusSession`**: `ClientSession`, `SessionManager`, `SessionPacketHandler` (despacho de opcodes).
- Documentação: [`Docs/NETWORK.md`](Docs/NETWORK.md), [`Docs/NETWORK_TESTS.md`](Docs/NETWORK_TESTS.md), [`Docs/NEXT_STEPS.md`](Docs/NEXT_STEPS.md). Cliente: [`ClientTest/README.md`](ClientTest/README.md).

- **`ZeusLog`** (implementação em **ZeusPlatform**): mutex partilhado com título e painel VT, para não corromper o cursor.
- Com **stderr redirecionado** (ficheiro/pipe), o painel in-place desliga-se de forma segura; o **ficheiro de sessão** continua a ser escrito se a pasta existir.
- **Windows Terminal** ou consola com VT ativo recomendado para `DebugOverlayRows` > 0.

## Ainda não implementado (fases seguintes)

Mundo, colisão, movimento, replicação, Jolt, plugin Unreal — ver [`Docs/NEXT_STEPS.md`](Docs/NEXT_STEPS.md).

## `_WIN32_WINNT`

Os projetos definem `WINVER` / `_WIN32_WINNT` como `0x0A00` (Windows 10) para APIs consistentes em servidor.
