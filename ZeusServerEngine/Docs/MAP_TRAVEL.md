# Map Travel — Servidor anuncia o mapa autoritativo apos o handshake

## Resumo

Logo apos o handshake UDP terminar (`S_CONNECT_OK`), o servidor envia o opcode **`S_TRAVEL_TO_MAP`** com o nome logico do mapa em execucao e o caminho Unreal completo. O cliente faz `UGameplayStatics::OpenLevel` para abrir esse mapa.

Cliente Unreal arranca por defeito em `/Game/ThirdPerson/Lobby` (ver `Clients/ZClientMMO/Config/DefaultEngine.ini`).

## Fluxo

```
Client (Lobby)                     Server
      |                              |
      | C_CONNECT_REQUEST            |
      |----------------------------->|
      |          S_CONNECT_CHALLENGE |
      |<-----------------------------|
      | C_CONNECT_RESPONSE           |
      |----------------------------->|
      |                S_CONNECT_OK  |
      |<-----------------------------|
      |             S_TRAVEL_TO_MAP  |  (mapName, mapPath, serverTimeMs)
      |<-----------------------------|
      |                              |
      | OpenLevel(mapPath)           |
```

Ambos os pacotes seguem em canal `Loading` com `ReliableOrdered`, garantindo a ordem.

## Wire format

Opcode: `S_TRAVEL_TO_MAP = 1040` (`Server/ZeusServerEngine/Source/ZeusProtocol/Public/Opcodes.hpp`).

Payload (UTF-8 little-endian):

| Campo | Tipo | Notas |
|-------|------|-------|
| `mapName` | string (uint16 length + UTF-8 bytes) | Nome logico (ex.: `"TestWorld"`). |
| `mapPath` | string (uint16 length + UTF-8 bytes) | Caminho Unreal completo (ex.: `"/Game/ThirdPerson/TestWorld"`). Pode ser vazio. |
| `serverTimeMs` | uint64 | Wall-clock do servidor no instante do envio. |

Implementado em `Server/ZeusServerEngine/Source/ZeusProtocol/Public/SessionWire.hpp` (`TravelToMapPayload`, `Read/WriteTravelToMapPayload`).

## Configuracao do servidor

`Server/ZeusServerEngine/Config/server.json`:

```json
"WorldDefaultMap": "TestWorld",
"WorldClientMapPathByName": {
  "TestWorld": "/Game/ThirdPerson/TestWorld",
  "Lobby": "/Game/ThirdPerson/Lobby"
}
```

Resolucao em `CoreServerApp::Initialize`:

1. Le `WorldDefaultMap` (default `"TestWorld"`).
2. Le objecto plano `WorldClientMapPathByName` (mesmo helper de `Collision.CollisionFileByMap`).
3. `clientMapPath = WorldClientMapPathByName[WorldDefaultMap]` (string vazia se ausente).
4. `mainMap->SetClientMapPath(clientMapPath)`.
5. `sessionPacketHandler.SetTravelInfo(WorldDefaultMap, clientMapPath)`.

Se ambos `mapName` e `mapPath` estiverem vazios o `SessionPacketHandler` nao envia o opcode.

## Envio

Em `Server/ZeusServerEngine/Source/ZeusSession/Private/SessionPacketHandler.cpp`:

- Caminho final do `C_CONNECT_RESPONSE` valido: imediatamente apos o `SendOne(S_CONNECT_OK)`.
- Caminho idempotente (`C_CONNECT_REQUEST` repetido com sessao ja `Connected`): tambem reenvia `S_TRAVEL_TO_MAP`.

Logs:

```
[Session] Connected sessionId=... connectionId=...
[Session] Sent S_TRAVEL_TO_MAP map=TestWorld path=/Game/ThirdPerson/TestWorld
```

## Cliente

- `Server/Plugins/Unreal/ZeusNetwork/Source/ZeusNetwork/Public/ZeusOpcodes.h`: `S_TRAVEL_TO_MAP = 1040` (espelho).
- `UZeusNetworkSubsystem::HandleIncomingPacket` parseia `(MapName, MapPath, ServerTimeMs)`, faz `EmitLog` e `OnServerTravelRequested.Broadcast(MapName, MapPath)`.
- Delegate Blueprint-assignable `FZeusOnServerTravelRequested` em `UZeusNetworkSubsystem`.
- `UZClientMMOGameInstance::HandleZeusServerTravelRequested` chama `UGameplayStatics::OpenLevel(this, FName(MapPath.IsEmpty() ? MapName : MapPath))`. Skip se ja estamos no mapa.

`Clients/ZClientMMO/Config/DefaultEngine.ini`:

```
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/ThirdPerson/Lobby.Lobby
EditorStartupMap=/Game/ThirdPerson/Lvl_ThirdPerson.Lvl_ThirdPerson
```

## Edge cases

- **Mapa em falta na config:** `WorldClientMapPathByName` sem entrada para o `WorldDefaultMap`. `mapPath = ""`. Cliente faz `OpenLevel(FName(MapName))` — Unreal aceita nomes simples se o mapa estiver disponivel pela short name resolution; caso contrario, log de warning do Unreal.
- **Reconexao:** o cliente reenvia `C_CONNECT_REQUEST` -> servidor reenvia `S_CONNECT_OK` + `S_TRAVEL_TO_MAP`. O cliente compara com `GetWorld()->GetMapName()` e ignora `OpenLevel` redundante.
- **Mapa diferente entre handshake e troca de servidor:** futuro `S_TRAVEL_TO_MAP` enviado fora do `C_CONNECT_RESPONSE` pode forcar nova viagem; ja suportado pelo handler do cliente, basta o servidor invocar `SetTravelInfo` + um envio explicito.

## Limitacoes / a melhorar

- Sem versionamento do mapa (cliente pode estar com asset diferente do servidor).
- Sem upload/streaming de `static_collision.zsm` ao cliente (Parte 4.5/Parte 5).
- Sem opcao para suprimir o envio automatico (ex.: handshake de auth antes de definir o mapa). Esperado em iteracao futura.
