# Próximos passos — Zeus Server Engine (rede e além)

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
2. **Parte 4 — Collision foundation** (`ZeusMapTools`, export de colisão, **Jolt**, `ZeusCollisionWorld` / `ZeusPhysicsWorld`).  
3. Parte 5 — Movement foundation  
4. Parte 6 — Spawn de player  
5. Parte 7 — Replication/AOI  

## Documentação

- Expandir exemplos de payloads quando novos opcodes forem adicionados.
- Manter `Docs/NETWORK.md` e `Docs/NETWORK_TESTS.md` sincronizados com o wire real.
- Manter `Docs/WORLD.md` e `Docs/WORLD_TESTS.md` com o comportamento do runtime de mundo.
