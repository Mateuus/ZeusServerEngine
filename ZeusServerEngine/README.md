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

Saída: `bin/Debug/` ou `bin/Release/` — executável `ZeusServer.exe`, pastas `Config/` e `Logs/` (criadas no pós-build), cópia do `server.json`.

### Linux (CMake)

Na pasta do servidor:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Executável: `build/ZeusServer`. Copiar ou apontar para `Config/server.json` (ex.: `cp -r Config build/` e correr a partir de `build/`).

## Executar

A partir da pasta de saída (para `Config/server.json` relativo funcionar):

```bat
cd bin\Debug
ZeusServer.exe
```

Ou passar o caminho do JSON:

```bat
ZeusServer.exe C:\caminho\para\server.json
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

## Parte 2 — rede (handshake + ping + disconnect)

- **`ZeusProtocol`**: wire + `SessionWire` (payloads connect/reject/ping/pong/disconnect).
- **`ZeusNet`**: `UdpSocket`, `UdpServer`, `UdpEndpoint`, `NetConnection`, `NetConnectionManager`.
- **`ZeusSession`**: `ClientSession`, `SessionManager`, `SessionPacketHandler` (despacho de opcodes).
- Documentação: [`Docs/NETWORK.md`](Docs/NETWORK.md), [`Docs/NEXT_STEPS.md`](Docs/NEXT_STEPS.md). Cliente: [`ClientTest/README.md`](ClientTest/README.md).

- **`ZeusLog`** (implementação em **ZeusPlatform**): mutex partilhado com título e painel VT, para não corromper o cursor.
- Com **stderr redirecionado** (ficheiro/pipe), o painel in-place desliga-se de forma segura; o **ficheiro de sessão** continua a ser escrito se a pasta existir.
- **Windows Terminal** ou consola com VT ativo recomendado para `DebugOverlayRows` > 0.

## Ainda não implementado (fases seguintes)

Filas de envio, reenvio reliable completo, mundo, colisão, movimento, replicação, Jolt, plugin Unreal — ver [`Docs/NEXT_STEPS.md`](Docs/NEXT_STEPS.md).

## `_WIN32_WINNT`

Os projetos definem `WINVER` / `_WIN32_WINNT` como `0x0A00` (Windows 10) para APIs consistentes em servidor.
