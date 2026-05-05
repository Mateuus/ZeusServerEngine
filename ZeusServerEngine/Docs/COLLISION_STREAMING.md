# COLLISION — Parte 4.5 (streaming, volumes dinâmicos, terreno)

Este documento complementa [COLLISION.md](COLLISION.md) com o que foi introduzido na **Parte 4.5**: três ficheiros binários por mapa, `ZeusRegionSystem`, `DynamicCollisionWorld` e exportação estendida no `ZeusMapTools`.

---

## 1. Três ficheiros `.zsm` por mapa

| Ficheiro | Magic | Conteúdo |
|----------|-------|----------|
| `static_collision.zsm` | `ZSMC` | Mesmo modelo da Parte 4: entidades com `AggGeom` simples. |
| `dynamic_collision.zsm` | `ZSMD` | Volumes (`AVolume` + tags `Zeus.*`): triggers, água, lava, kill, safe zone. Shapes iguais ao estático. |
| `terrain_collision.zsm` | `ZSMT` | `TriangleMesh` (mesh com tag `Zeus.TriangleMesh` ou `-AllowComplexAsSimple`) e `HeightField` (landscape via `FillHeightTile`). |

O servidor resolve caminhos por mapa em `server.json`:

- `Collision.CollisionFileByMap`
- `Collision.DynamicCollisionFileByMap`
- `Collision.TerrainCollisionFileByMap`

---

## 2. `debug_collision.json` (v1 estendido)

Além de `entities[]`, o JSON inclui:

- `volumes[]` — nome, `kind`, `eventTag`, `region`, `bounds`, `worldTransform`, `shapes[]`.
- `terrainPieces[]` — resumo por peça (`triangleMesh` com contagens ou `heightField` com `samplesX/Y`, etc.).

`stats` inclui `volumeCount`, `terrainPieceCount`, `triangleMeshCount`, `heightFieldCount`.

---

## 3. Region streaming (servidor)

- **`CollisionAsset::EntityIndexByRegion`** e equivalentes em `DynamicCollisionAsset` / `TerrainCollisionAsset` são preenchidos no load.
- **`CollisionWorld::LoadRegion` / `UnloadRegion`**: cria/remove bodies Jolt por `RegionId`; terreno partilha o mesmo mapa `BodiesByRegion`.
- **`DynamicCollisionWorld`**: sem Jolt; activa índices por região e responde a `QueryAt` / `FindByEventTag`.
- **`ZeusRegionSystem`** (módulo `ZeusWorld`): lê actores com tag `PlayerProxy` (ex.: `DebugPlayerActor`), calcula grelha com o mesmo hash que o exporter, aplica raio + histerese e chama load/unload nos dois mundos.

Configuração típica em `server.json`:

```json
"RegionStreaming": {
  "Enabled": true,
  "StreamingRadiusCm": 15000,
  "UnloadHysteresisCm": 2000,
  "MaxLoadsPerTick": 4,
  "MaxUnloadsPerTick": 4
},
"DebugPlayer": {
  "Enabled": true,
  "SpeedCmS": 800,
  "Waypoints": [[0,0,200], [5000,0,200]]
}
```

---

## 4. Tags Unreal (volumes)

| Tag | `ZSMD` kind |
|-----|-------------|
| *(nenhuma, `ATriggerVolume`)* | Trigger |
| `Zeus.Water` | Water |
| `Zeus.Lava` | Lava |
| `Zeus.KillVolume` | KillVolume |
| `Zeus.SafeZone` | SafeZone |

Opcional: tag `tag:NomeEvento` define `EventTag` exportado.

---

## 5. Smoke tests extra

- **TC-06 .. TC-09** (`CollisionTestScene::RunStreamingScenarios`): load/unload, consistência, `QueryAt` em volume dinâmico, raycast em heightfield programático.
- **`RegionSystemTests`**: grelha 3×3 com `DebugPlayerActor` simulado.

---

## 6. Limitações conhecidas

- **HeightField Jolt**: `SamplesX` deve igualar `SamplesY` (validação no `JoltShapeFactory`).
- **Landscape**: alturas vêm de `ULandscapeHeightfieldCollisionComponent::FillHeightTile` (build Editor); múltiplos `LandscapeComponent` geram várias peças no `ZSMT`.
- **`physics_settings.json`**: não faz parte desta entrega; ver backlog **BL-004**.
