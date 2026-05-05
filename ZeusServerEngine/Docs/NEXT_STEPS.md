# Próximos passos — Zeus Server Engine (rede e além)

## Concluído (Map Travel pós-handshake)

- Novo opcode `S_TRAVEL_TO_MAP = 1040` (servidor → cliente) com payload `mapName + mapPath + serverTimeMs`.
- Servidor envia logo após `S_CONNECT_OK` (caminho final + idempotente).
- `MapInstance::ClientMapPath` + config `WorldClientMapPathByName` em `server.json`.
- Cliente Unreal: delegate `OnServerTravelRequested`, `UZClientMMOGameInstance` faz `OpenLevel`. `GameDefaultMap=/Game/ThirdPerson/Lobby.Lobby`.
- Documentação: [MAP_TRAVEL.md](MAP_TRAVEL.md). ADR **0012** em `.brain/decisoes/adrs/`.

## Concluído (Parte 4 — Collision Foundation)

- Plugin Editor **`ZeusMapTools`**, `ZSMC` v1, `ZsmCollisionLoader`, Jolt, `CollisionWorld`, TC-01..TC-05.
- ADR **0011**.

## Concluído (Parte 4.5 — streaming + dinâmico + terreno)

- **`ZeusMapTools`**: `dynamic_collision.zsm` (`ZSMD` v1), `terrain_collision.zsm` (`ZSMT` v1), `debug_collision.json` com `volumes[]` e `terrainPieces[]`; commandlet com `-BuildDynamicZsm`/`-SkipDynamicZsm`, `-BuildTerrainZsm`/`-SkipTerrainZsm`.
- **Servidor**: `ZsmDynamicLoader`, `ZsmTerrainLoader`, `DynamicCollisionWorld`, `CollisionWorld::LoadRegion`/`UnloadRegion`, terreno em Jolt (`MeshShape`/`HeightFieldShape`), `ZeusRegionSystem`, `DebugPlayerActor`, TC-06..TC-09 + `RegionSystemTests`.
- Documentação: [COLLISION_STREAMING.md](COLLISION_STREAMING.md); ADR **0013**; backlog **BL-002** fechado.

## Concluído (Parte 5 — Movement Foundation)

- Novo módulo **`ZeusGame`** (`Entities`, `Movement` implementados; `Combat`/`Spells`/`Skills`/`AuraStatus`/`Inventory`/`LagCompensation`/`Rules` como roadmap).
- `CharacterActor`, `MovementComponent` (sweep & slide, gravidade, walkable floor, step up, substepping vertical, slide off em rampa íngreme), `MovementSystem` (config + stats), `MovementTests` (10 cenários TC-MOVE-001..010, todos OK).
- `PhysicsWorld::SweepCapsule` (Jolt `CastShape`) + `CollisionWorld` façade (`Raycast`/`RaycastDown`/`CollideCapsule`/`SweepCapsule`); cápsula Z-up via `RotatedTranslatedShape`.
- `server.json` ganha bloco `Movement` (`Enabled`, `RunSmokeTests`, `SpawnDebugCharacter`, `GravityZ`, `MaxSlopeAngleDeg`, `StepHeightCm`, `DefaultSpeedCmS`, `JumpVelocityCmS`, `Capsule.RadiusCm`/`HalfHeightCm`).
- `CoreServerApp` integra `MovementSystem`; `ConsoleLivePanel` slot 3 mostra `pos/vel/grounded` ou stats agregadas.
- Documentação: [MOVEMENT.md](MOVEMENT.md); ADR **0014**; backlog **BL-003** fechado.

### A seguir

- **`physics_settings.json`** e integração de eventos de volume com gameplay (**BL-004**).
- **Parte 6 — Spawn de player** (PlayerController + Possess + loading channel).

## Concluído nesta fase (Parte 3.5 — plugin Unreal ZeusNetwork)

- Plugin **`ZeusNetwork`** em `Server/Plugins/Unreal/ZeusNetwork/` (Runtime): `UZeusNetworkSubsystem` (UDP, handshake v2, ping/pong, disconnect, delegates Blueprint, `LogZeusNetwork` + log em `Saved/Logs/ZeusNetwork/session.log`).
- Projeto cliente `Clients/ZClientMMO`: `AdditionalPluginDirectories` + `ZeusNetwork` ativo no `.uproject`; ver [README do plugin](../../Plugins/Unreal/ZeusNetwork/README.md).
- ADR **0010** em `.brain/decisoes/adrs/`.

## Concluído nesta fase (Parte 3 — ZeusWorld + ZeusMath)

- Módulos **`ZeusMath`** e **`ZeusWorld`** (tipos 3D, `Actor`/`ActorComponent`, `EntityManager`, `World`, `Level`, `MapInstance`, `WorldRuntime`, `WorldStats`).
- **`CoreServerApp`**: tick fixo unificado (rede opcional + sessão + mundo), ver [WORLD.md](WORLD.md).
- Config: `WorldDefaultMap`, `WorldDebugSpawnActors` em `Config/server.json`.
- Documentação: [WORLD.md](WORLD.md), [WORLD_TESTS.md](WORLD_TESTS.md).

## Concluído anteriormente (Parte 2.5 — validação / hardening)

- Handshake v2 com `S_CONNECT_REJECT` em `C_CONNECT_RESPONSE` inválido (`InvalidHandshake` / `InvalidPacket`).
- Validação de `clientBuild` (não vazio, máx. 256 bytes).
- Fila de reordenação **`ReliableOrdered`** por canal (`SubmitInboundReliableOrdered`).
- Limites de fragmentação / reassembly configuráveis.
- **`ConnectionTimeoutMs`** e logs de timeout.
- Reliable: desistência após `ReliableMaxResends`, remoção de conexão em falha agregada, logs throttled.
- **`NetworkDebugAck`** para `[NetAck]`.
- **ClientTest** estendido (`--auto`, comandos `reliable`, `ordered`, `fragment`, etc.).
- **`Docs/NETWORK_TESTS.md`** e `Docs/NETWORK.md` alinhados ao código.

## Rede / transporte (melhorias incrementais futuras)

- `S_DISCONNECT_OK` automático opcional ao expirar timeout (hoje: remoção + logs).
- Testes automatizados em CI (wire + ClientTest + cenários com `NetSim`).
- `NetReceiveQueue` explícita se o motor precisar de priorização na receção.
- **Simulador:** latência, jitter, duplicados e reordenação no `NetworkSimulator` (hoje só `NetSimDropPermille`).
- **NAT traversal**, **encryption**, **compression**, **congestion control**, **advanced jitter buffer**, **bandwidth budget**, **packet priority** — não iniciar na Parte 2.5; apenas planeamento.

## Gameplay / motor (não misturar com rede nesta base)

- Player, Character, Pawn, movimento, replicação, AOI, Jolt, ZeusMapTools, inventário, combate, NPC, quest, plugin Unreal.

## Ordem recomendada

1. ~~**Parte 3 — World mínimo**~~ (ZeusMath + ZeusWorld + integração app).  
2. ~~**Parte 4 — Collision foundation**~~ — `ZeusMapTools`, ZSM, Jolt, `CollisionWorld`/`PhysicsWorld`, smoke tests.  
3. ~~**Parte 4.5** — Dynamic collision + terrain + region streaming.~~  
4. ~~**Parte 5 — Movement foundation**~~ — `ZeusGame` + `CharacterActor` + `MovementComponent` + sweep & slide + step up.  
5. Parte 6 — Spawn de player.  
6. Parte 7 — Replication/AOI.  

## Documentação

- Expandir exemplos de payloads quando novos opcodes forem adicionados.
- Manter `Docs/NETWORK.md` e `Docs/NETWORK_TESTS.md` sincronizados com o wire real.
- Manter `Docs/WORLD.md` e `Docs/WORLD_TESTS.md` com o comportamento do runtime de mundo.
- Manter `Docs/COLLISION.md` e `Docs/COLLISION_TESTS.md` em linha com o ZSMC (versão, layout) e com `CollisionTestScene`.
- Manter `Docs/MOVEMENT.md` em linha com os parâmetros do `MovementComponent` e os 10 TC-MOVE.
