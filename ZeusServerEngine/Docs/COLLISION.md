# COLLISION — Parte 4 (Collision Foundation)

Este documento descreve a fundação de colisão autoritativa do Zeus Server Engine: pipeline de exportação a partir da Unreal, formatos `debug_collision.json` e `static_collision.zsm`, módulo `ZeusCollision` e integração com Jolt Physics.

A Parte 4 prova apenas o pipeline `Unreal → ZSM → Jolt`, com smoke tests de chão / parede / rampas. Movimento real, character controller, predição e replicação ficam para a Parte 5.

**Parte 4.5** (streaming, `dynamic_collision.zsm`, `terrain_collision.zsm`, `ZeusRegionSystem`): ver [COLLISION_STREAMING.md](COLLISION_STREAMING.md) e ADR **0013** em `.brain/decisoes/adrs/`.

**Parte 5** (Movement foundation: cápsula servidor-only + sweep & slide + gravidade + step up no `MovementComponent` do novo módulo `ZeusGame`): ver [MOVEMENT.md](MOVEMENT.md) e ADR **0014**. Inclui o sweep `PhysicsWorld::SweepCapsule` (Jolt `CastShape`).

---

## 1. Princípios

- O **servidor é autoritativo**: define onde estão as colisões e como uma cápsula de teste interage com elas.
- O servidor **não conhece** Unreal Engine. Só lê dois ficheiros: `debug_collision.json` (referência humana) e `static_collision.zsm` (binário oficial).
- A fonte real de colisão na Unreal é `UStaticMeshComponent → UStaticMesh → UBodySetup → FKAggregateGeom` (Box/Sphere/Sphyl/Convex). O overlay amarelo é apenas referência visual.
- Unidades: 1 unidade Zeus = 1 cm, com Z-Up (alinhado a Unreal). Jolt corre em metros; a conversão acontece num único sítio (`JoltConversion`).
- Sem dependência directa entre Zeus e Jolt fora do módulo `ZeusCollision` — qualquer outro módulo (Math/World/App) lida apenas com `CollisionWorld` e tipos engine-agnostic.

---

## 2. Pipeline

```
Unreal .umap
   │  (UStaticMesh + UBodySetup + AggGeom)
   ▼
ZeusMapTools (Editor plugin / Commandlet)
   ├── debug_collision.json    (legível, halfExtent, stats)
   └── static_collision.zsm    (binário ZSMC v1, little-endian)
   │
   ▼  (read by server, no Unreal dependency)
ZsmCollisionLoader  ──>  CollisionAsset  ──> CollisionWorld
                                              │
                                              ▼
                                         PhysicsWorld (Jolt)
                                              │
                                              ▼
                              Raycast / CollideCapsule / Step
```

---

## 3. Formato `debug_collision.json` (v1)

Identificadores:

```json
{
  "format": "zeus_debug_collision",
  "version": 1,
  "mapName": "TestWorld",
  "units": "centimeters",
  "coordinateSystem": { "x": "forward", "y": "right", "z": "up" }
}
```

Cada entidade contém:

- `entityName`, `actorName`, `componentName`, `sourcePath`.
- `entityWorldTransform.location/rotation/scale` (rotation = quaternion `[x, y, z, w]`).
- `bounds.center/extent` em cm.
- `region.regionId`, `gridX/Y/Z`, `regionSizeCm`.
- `shapes[]` com:
  - `type` ∈ `box | sphere | capsule | convex`.
  - `localTransform` igual ao da entidade.
  - **Box**: `extentSemantics: "halfExtent"` + `halfExtents: [hx, hy, hz]`.
  - **Sphere**: `radius` (cm).
  - **Capsule**: `radius` + `halfHeight` (cm).
  - **Convex**: `vertices: [[x, y, z], ...]` em local space (cm).
  - `warnings: []`.

`stats` agrega contagens por tipo, `warningCount`, `skippedActorCount` etc., para validar o `static_collision.zsm` companheiro.

---

## 4. Formato `static_collision.zsm` (ZSMC v1)

Tudo little-endian, gravado **campo a campo** (sem padding). Strings: `uint16` length + `N` bytes UTF-8.

### Header (32 bytes fixos + mapName)

| Offset | Tipo | Campo |
|--------|------|-------|
| 0  | u8 ×4 | Magic = `'Z','S','M','C'` |
| 4  | u16   | Version = 1 |
| 6  | u16   | HeaderSize = 32 |
| 8  | u32   | Flags (`bit0=HasRegions`) |
| 12 | u32   | EntityCount |
| 16 | u32   | ShapeCount |
| 20 | u32   | RegionSizeCm |
| 24 | u32   | BoxCount |
| 28 | u32   | SphereCount |
| 32 | u32   | CapsuleCount |
| 36 | u32   | ConvexCount |
| 40 | str   | MapName (`u16 len + UTF-8`) |

### Per entity

```
str  name                  (u16 len + UTF-8)
str  actorName
str  componentName
f64  location.x / y / z
f64  rotation.x / y / z / w
f64  scale.x / y / z
f64  boundsCenter.x / y / z
f64  boundsExtent.x / y / z
u32  regionId
i32  gridX / gridY / gridZ
u32  shapeCount
shape[shapeCount]
```

### Per shape

```
u8   type (1=Box, 2=Sphere, 3=Capsule, 4=Convex)
f64  local.location.x / y / z
f64  local.rotation.x / y / z / w
f64  local.scale.x / y / z
payload (per type)
```

Payload por tipo:

- **Box** → `f64 halfExtents.x / y / z` (cm).
- **Sphere** → `f64 radius` (cm).
- **Capsule** → `f64 radius` + `f64 halfHeight` (cm).
- **Convex** → `u32 vertexCount` + `vertexCount × (f64 x, f64 y, f64 z)` (cm, local).

`TriangleMesh` (type=5) e `HeightField` ficam reservados para a Parte 4.5.

---

## 5. Módulo `ZeusCollision` (servidor)

```
ZeusCollision
├── Public/
│   ├── CollisionShapeType.hpp           — enum (Box/Sphere/Capsule/Convex)
│   ├── CollisionShape.hpp               — POD para todas variantes
│   ├── CollisionEntity.hpp              — entidade + WorldTransform + Bounds + Region
│   ├── CollisionAsset.hpp               — asset (entidades + stats + meta)
│   ├── ZsmCollisionFormat.hpp           — constantes do ZSMC v1
│   ├── ZsmCollisionLoader.hpp           — parser binário
│   ├── JoltConversion.hpp               — cm <-> m, Quat, RVec3
│   ├── JoltShapeFactory.hpp             — CollisionShape -> JPH::Shape
│   ├── PhysicsWorld.hpp                 — wrapper Jolt (init / queries)
│   ├── CollisionWorld.hpp               — orquestra Asset + PhysicsWorld
│   ├── CollisionDebug.hpp               — helpers de log
│   └── CollisionTestScene.hpp           — smoke tests TC-01..TC-05
└── Private/
    └── ... (.cpp correspondentes)
```

### Boundary rules

- Headers `*.hpp` em `Public/` **não** podem incluir `<Jolt/...>`. As únicas excepções são `JoltConversion.hpp` e `JoltShapeFactory.hpp`, e o `PhysicsWorld.hpp` mantém `JPH::*` apenas dentro do `PimplState` privado.
- `ZeusMath` continua engine-agnostic. `IsWalkableFloor` é o único helper de inclinação adicionado.
- `ZeusWorld` ainda **não** depende de `ZeusCollision` na Parte 4 — quem decide chamar é o `CoreServerApp`.

---

## 6. Integração com `CoreServerApp`

Gates em `Config/server.json`:

```json
"Collision": {
  "EnableCollisionWorld": false,
  "RunCollisionSmokeTest": false,
  "CollisionFileByMap": {
    "TestWorld": "Data/Maps/TestWorld/static_collision.zsm",
    "AnotherMap": "Data/Maps/AnotherMap/static_collision.zsm"
  },
  "CollisionFile": "Data/Maps/_fallback/static_collision.zsm"
}
```

**Resolução do ficheiro `.zsm` (alinhado a `WorldDefaultMap`):**

1. Lê-se `WorldDefaultMap` (ex.: `TestWorld`).
2. Se existir entrada em `Collision.CollisionFileByMap` com essa chave, usa-se esse caminho.
3. Senão, se `Collision.CollisionFile` estiver preenchido (legado / fallback único), usa-se esse caminho para qualquer mapa ainda não listado.
4. Senão, convenção: `Data/Maps/<WorldDefaultMap>/static_collision.zsm`.

No arranque o servidor regista: `Collision path resolved map=<nome> zsm=<caminho>`.

`CollisionFileByMap` é um objecto JSON plano (sem nesting); o parser mínimo do `CoreServerApp` só suporta esse formato dentro do bloco `Collision`.

Ordem em `CoreServerApp::Initialize`:

1. Logs / sessão / network / `WorldRuntime` como antes.
2. Se `EnableCollisionWorld=true`, criar `CollisionWorld` e tentar `LoadFromZsm` no caminho resolvido acima.
3. Se o ficheiro não existir mas `RunCollisionSmokeTest=true`, usar `CollisionTestScene::BuildProgrammaticAsset()`.
4. `BuildPhysicsWorld()` → cria `PhysicsWorld` (Jolt) + bodies estáticos + `OptimizeBroadPhase()`.
5. Se `RunCollisionSmokeTest=true`, correr `CollisionTestScene::RunAll()` e logar `passed/failed`.

`Shutdown` desliga primeiro `CollisionWorld` (que limpa bodies + `JPH::Factory`), depois `WorldRuntime`, depois UDP/sessions como antes.

---

## 7. Logs esperados

```
[Collision] Loading Data/Maps/TestWorld/static_collision.zsm
[Collision] Loaded entities=N shapes=M (box=B sphere=S capsule=C convex=K) ...
[PhysicsWorld] Initialized Jolt physics system
[Collision] Static bodies created=N skipped=0
[CollisionTest] TC-01 Capsule vs floor (RaycastDown): OK (distance=...)
[CollisionTest] TC-02 Capsule vs wall (CollideCapsule): OK
[CollisionTest] TC-03 Walkable ramp (~30 deg): OK ...
[CollisionTest] TC-04 Too steep ramp (~70 deg) blocks: OK ...
[CollisionTest] TC-05 Lateral raycast vs wall: OK ...
[CollisionTest] Summary passed=5 failed=0
[App] CoreServerApp initialized (Part 1 + Part 2 network + Part 3 world runtime + Part 4 collision).
```

---

## 8. Limites e próximos passos

- A Parte 4.5 entregou `dynamic_collision.zsm`, `terrain_collision.zsm`, streaming por região e export estendido; detalhe em [COLLISION_STREAMING.md](COLLISION_STREAMING.md).
- **`physics_settings.json`** (gravidade, materiais, escalas globais) e encadeamento gameplay de eventos de volume ficam para **BL-004** / Parte 5.
- Sem `Character Controller` completo: `DebugPlayerActor` serve apenas para validar streaming.
- Sem profiling Jolt (`JPH_PROFILE_ENABLED` desactivado). Pode ser ligado em build de diagnóstico.
