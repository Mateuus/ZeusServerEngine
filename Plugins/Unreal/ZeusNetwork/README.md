# ZeusNetwork (Unreal Engine 5.7)

Plugin **Runtime** mínimo: cliente UDP para o Zeus Server Engine — handshake v2, ping/pong (RTT), disconnect e logs. Não inclui movement, spawn, replicação nem gameplay.

## Onde fica e como o ZClientMMO encontra o plugin

- Plugin: `Server/Plugins/Unreal/ZeusNetwork/`
- O projeto [ZClientMMO.uproject](../../Clients/ZClientMMO/ZClientMMO.uproject) declara:

```json
"AdditionalPluginDirectories": [
  "../../Server/Plugins/Unreal"
],
"Plugins": [
  { "Name": "ZeusNetwork", "Enabled": true }
]
```

A Unreal descobre `ZeusNetwork.uplugin` nessa pasta extra.

## Ativar e compilar

1. Abrir `Clients/ZClientMMO/ZClientMMO.uproject` na UE 5.7.
2. Confirmar em **Edit → Plugins → Zeus → Zeus Network** que está ativo (já vem `Enabled` no `.uproject`).
3. **Development Editor | Win64**: deixar a UE gerar/compilar o módulo `ZeusNetwork`, ou clicar com o botão direito no `.uproject` → **Generate Visual Studio project files** e compilar na VS.

## Project Settings

**Edit → Project Settings → Plugins → Zeus Network** (classe `UZeusNetworkSettings`):

- `DefaultHost` (default `127.0.0.1`)
- `DefaultPort` (default `27777`)
- `ConnectTimeoutSeconds` (default `30`)
- `PingIntervalSeconds` (default `1`) — enquanto `Connected`, o subsystem envia `C_PING` automaticamente neste intervalo; também podes chamar `SendPing()` manualmente.

## Blueprint — teste rápido

1. Abre um mapa qualquer (ex. nível default).
2. Define uma **Game Instance** Blueprint (ex. `BP_ZeusClientGameInstance`) em **Project Settings → Maps & Modes → Game Instance Class**.
3. **Não ligues o Zeus no `Event Init` da Game Instance.** Na UE, `ReceiveInit` corre **antes** de `SubsystemCollection.Initialize` — os `UGameInstanceSubsystem` (incluindo ZeusNetwork) **ainda não existem**, por isso `Get Zeus Network Subsystem` e `Get Game Instance Subsystem` devolvem **None**.
4. Fluxo correcto na Game Instance Blueprint:
   - **Event Init** → nó **Delay** com duração **0.0** (executa no tick seguinte, já com subsistemas criados) → **`Get Zeus Network Subsystem`** com **`Self`** no *World Context* → **Connect** / **Bind Event** …  
   - Ou coloca a lógica no **BeginPlay** do **Game Mode** (ou Level Blueprint): **Get Game Instance** → **Get Zeus Network Subsystem** (`Self` = Game Instance obtida).
5. **Assign OnConnected** / **On Connection Failed** / **On Network Log** para veres o resultado no ecrã ou no Output Log.
6. **Connect To Zeus Server** (`127.0.0.1`, `27777`) ou **Connect To Default Zeus Server**.
7. Garante o servidor a escutar UDP na mesma porta (ex. `ListenUdpPort` em `Config/server.json`).

## Logs

- **Output Log**: categoria `LogZeusNetwork`, linhas com prefixo `[ZeusNetwork] ...` (ex.: `Opening UDP socket`, `Sending C_CONNECT_REQUEST`, `Received S_CONNECT_CHALLENGE`, `Sending C_CONNECT_RESPONSE`, `Connected connectionId=...`, `Sending C_PING`, `S_PONG RTT=...ms`, `Disconnecting`, `Disconnected`).
- **Ficheiro de sessão** (espelho para análise offline, semelhante ao log de sessão do servidor):  
  `Saved/Logs/ZeusNetwork/session.log` (relativo ao projeto do cliente, por exemplo `ZClientMMO/Saved/...`).

## Wire / protocolo

- Cabeçalho **32 bytes** LE, magic `ZEUS`, versão **2**, opcodes alinhados a `ZeusServerEngine/Source/ZeusProtocol/Public/Opcodes.hpp` e a [Docs/NETWORK.md](../../ZeusServerEngine/Docs/NETWORK.md).
- `C_CONNECT_REQUEST`: canal **Loading**, entrega **Reliable** (como o ClientTest).
- `C_CONNECT_RESPONSE`: **Loading** + **ReliableOrdered**; antes de cada envio físico repõe-se o order id do canal Loading a **0** (retransmissão após perda UDP).
- ACK / `ackBits`: lógica espelhada a `NetConnection::MarkReceived` no servidor.

## Limitações

- Sem reliable genérico nem fragmentação loading no cliente (só o necessário ao handshake/ping/disconnect).
- `GetSessionId()` em Blueprint é `int64`; valores muito grandes usam `LexToString`/logs para o `uint64` completo se necessário.

## Próximo passo sugerido

Parte 4 — Collision foundation (ZeusMapTools + Jolt), conforme roadmap do motor.
