# ZeusWorld e ZeusMath — Parte 3

## 1. Objetivo do ZeusWorld

O módulo **ZeusWorld** é o runtime lógico do servidor: mapas instanciados, `World`, `Level`, entidades (`Actor`), gestão de IDs, spawn/despawn e tick de alto nível **sem** UDP, sessão detalhada, física ou replicação. Serve de base para movimento, colisão e replicação futuros.

## 2. Por que ZeusMath é separado

**ZeusMath** concentra tipos numéricos (`Vector3`, `Rotator`, `Quaternion`, `Transform`, `Aabb`, `SphereBounds`) reutilizáveis por World, Collision, Movement, Replication e debug. Assim evitamos dependências de `Actor`/`World` em matemática e mantemos o núcleo geométrico estável e testável.

## 3. Unidade oficial: centímetros

**1 unidade Zeus = 1 cm** (alinhado à Unreal). Posições, bounds e velocidades futuras usam cm.

## 4. Sistema de coordenadas

- **Eixo X** = Forward  
- **Eixo Y** = Right  
- **Eixo Z** = Up  

## 5. Relação conceitual com Unreal

Nomes e fluxos inspiram-se em **Unreal Engine 5.x** (`AActor`, `UActorComponent`, `UWorld`, `ULevel`, `FTransform`, etc.) **sem** copiar código Epic. A implementação é própria do Zeus.

## 6. Lifecycle do Actor

Estados (`EActorLifecycleState`): `Uninitialized` → `OnSpawned` (`Spawning`) → `BeginPlay` (`Active`) → `Destroy` (`PendingDestroy`) → `EndPlay` (`Destroyed`) → remoção do `EntityManager`.

## 7. Lifecycle do ActorComponent

`OnRegister` ao ser adicionado ao dono; `BeginPlay` quando o `Actor` entra em play; `TickComponent` se tick ativo; `EndPlay` no encerramento; `OnUnregister` ao destruir o componente/dono.

## 8. World, Level, MapInstance, WorldRuntime

- **WorldRuntime**: agrega uma ou mais `MapInstance`; `Tick` global do mundo.  
- **MapInstance**: instância de mapa (ID, nome, estado); contém um **World**.  
- **World**: nome, tempo simulado, `EntityManager`, lista de **Level**.  
- **Level**: camada lógica (nesta fase stub: `Load`/`Unload`/`Tick`).

## 9. EntityManager

Cria `Actor` (template `SpawnActor<T>`), atribui `EntityId`, aplica `SpawnParameters`, `TickActors`, fila `PendingDestroy` e `FlushPendingDestroy`.

## 10. O que ainda não está implementado

Player/Pawn/Character, `GameMode`/`GameState`, movimento autoritativo, colisão/Jolt, replicação/AOI, `ZeusMapTools`, inventário, combate, NPC, quest, plugin Unreal.

## 11. Preparação para Collision, Movement e Replication

- **Collision / AOI**: `Aabb` e `SphereBounds` para broadphase e debug.  
- **Movement / Replication**: `Transform` com rotação em quaternion; `EntityId` estável para canais futuros.  
- Comentários nos módulos mantêm fronteiras: World não conhece UDP nem Jolt.

## 12. Integração futura com Jolt (sem contaminar ZeusMath/ZeusWorld)

- Unidades Zeus permanecem em **cm**.  
- Jolt usará **metros** numa camada futura (ex.: `ZeusCollision` / `JoltConversion`: `ZeusToJolt = cm * 0.01`, `JoltToZeus = m * 100`).  
- **Não** colocar conversão Jolt dentro de `Vector3`, `Transform` ou `Actor`.

---

## Euler e Quaternion

`Rotator` está em **graus** (Pitch/Yaw/Roll). `Transform` guarda rotação como **Quaternion**. As funções `Quaternion::FromRotator` / `ToRotator` usam uma ordem aproximada Roll→Pitch→Yaw coerente entre si; perto dos **gimbal locks** ou ângulos extremos pode haver **divergência** face ao pipeline completo da Unreal — documentado para revisão na Parte 4+.

## Configuração (`Config/server.json`)

- **`WorldDefaultMap`**: nome lógico do mapa inicial (ex.: `"TestWorld"`).  
- **`WorldDebugSpawnActors`**: se `true`, spawna três `Actor` de debug após `BeginPlay` (ver logs em `Docs/WORLD_TESTS.md`).

## Tick do servidor

`CoreServerApp` regista **sempre** um callback de tick fixo no `EngineLoop`: rede (se UDP ativo), timeouts de sessão e `WorldRuntime::Tick`, mesmo com `ListenUdpPort = 0`.
