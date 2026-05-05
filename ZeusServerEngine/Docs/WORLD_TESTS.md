# Testes manuais — ZeusWorld / ZeusMath (Parte 3)

Checklist mínimo (sem runner C++ dedicado nesta fase). Marcar após verificação no ambiente.

| ID | Descrição |
|----|-----------|
| TEST-WORLD-001 | **Vector3**: soma, subtração, `Dot`, `Cross`, `Distance` — resultado coerente (ex.: `Dot(Forward,Right)==0`). |
| TEST-WORLD-002 | **Transform**: `Transform::Identity` mantém ponto; `Combine` de dois identidades = identidade. |
| TEST-WORLD-003 | **Aabb**: `Contains` ponto interior; `Intersects` entre dois AABBs sobrepostos. |
| TEST-WORLD-004 | **EntityManager**: `SpawnActor<Actor>` cria ator com `EntityId` válido e nome esperado. |
| TEST-WORLD-005 | **EntityManager**: `FindActor(id)` devolve o mesmo ponteiro após spawn. |
| TEST-WORLD-006 | **DestroyActor**: ator fica `IsPendingDestroy()` e aparece em contagem pendente até ao flush. |
| TEST-WORLD-007 | **FlushPendingDestroy**: após tick, ator removido; `FindActor` devolve `nullptr`. |
| TEST-WORLD-008 | **World**: após `BeginPlay`, `HasBegunPlay()` é `true` e atores em `Spawning` passam a `Active`. |
| TEST-WORLD-009 | **World Tick**: com `bAllowTick` e tick ativos no `Actor`, `Tick` é chamado (ex.: log temporário ou contador). |
| TEST-WORLD-010 | **WorldRuntime**: `CreateMapInstance` + `GetMainMapInstance()` devolve instância com `GetWorld()` inicializado. |

## Smoke de arranque (logs)

1. Definir em `Config/server.json`: `"WorldDebugSpawnActors": true`, ou copiar [server-world-smoke.example.json](../Config/server-world-smoke.example.json) para a pasta `Config` da saída do build e passar o caminho ao executável.  
2. Executar `ZeusServer.exe` (ou caminho equivalente) com CWD onde exista `Config/server.json` (ou o JSON escolhido como primeiro argumento).  
3. Confirmar linhas na consola (ordem aproximada):

- `[WorldRuntime] Initialized`  
- `[MapInstance] Created map=TestWorld` (ou valor de `WorldDefaultMap`)  
- `[World] Initialized name=...`  
- `[World] BeginPlay`  
- `[World] Spawned Actor entityId=... name=DebugActor_01` … `DebugActor_03`  
- `[World] Tick actors=3` (primeiro tick do mundo)  
- Ao encerrar: `[WorldRuntime] Shutdown`  

4. Repor `WorldDebugSpawnActors` a `false` se não for desejado por defeito.

## Regressão de rede

Com a mesma configuração de rede da Parte 2.5, executar os fluxos descritos em [NETWORK_TESTS.md](NETWORK_TESTS.md) e o **ClientTest** — o tick unificado não deve alterar o wire.
