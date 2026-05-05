# ZeusMapTools — plugin Unreal (Editor)

Plugin Editor + Commandlet usado para exportar a colisão simples de um nível Unreal para o formato lido pelo Zeus Server Engine (Parte 4 — Collision Foundation).

## Decisão oficial

- O servidor **não** carrega `.umap`.
- A fonte real de colisão é `UStaticMeshComponent → UStaticMesh → UBodySetup → FKAggregateGeom`.
- O overlay amarelo da Unreal é apenas referência visual; o que conta para o servidor são as `Box/Sphere/Sphyl/Convex` da agregação simples.

## Habilitar no projeto

Em `Clients/ZClientMMO/ZClientMMO.uproject`, garantir a entrada:

```json
"Plugins": [
  { "Name": "ZeusMapTools", "Enabled": true }
]
```

E que `AdditionalPluginDirectories` aponta para `Server/Plugins/Unreal` (já configurado para o `ZeusNetwork`).

## Menu Slate

`Main Menu → Ferramentas (Tools) → Zeus Map Tools` traz quatro acções:

- **Export Selected Collision** — exporta os atores actualmente selecionados (`debug_collision.json` + `static_collision.zsm`) e desenha debug.
- **Export Current Level Collision** — varre `GWorld` com `TActorIterator<AActor>` e exporta tudo que tiver `UStaticMeshComponent` válido.
- **Preview Selected Collision** — apenas debug draw, sem escrever ficheiros (Verde = exportável; Amarelo = exportável com warning; Vermelho = sem colisão; Azul = região).
- **Validate Selected Collision** — corre o pipeline em modo dry-run e lista os warnings em `LogZeusMapTools`.

## Commandlet headless

Comando exemplo:

```
"D:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  Clients\ZClientMMO\ZClientMMO.uproject ^
  -run=ZeusMapExport ^
  -Map=/Game/Maps/TestWorld ^
  -Output=Server\ZeusServerEngine\Data\Maps\TestWorld ^
  -DebugJson -BuildZsm -RegionSize=5000
```

Switches/parâmetros:

| Parâmetro | Descrição |
|-----------|-----------|
| `-Map=` | Caminho `/Game/...` para o `.umap` (obrigatório). |
| `-Output=` | Pasta de saída. Default: `<Project>/../Server/ZeusServerEngine/Data/Maps/<MapName>/`. |
| `-MapName=` | Sobrescreve `mapName` gravado nos ficheiros. |
| `-DebugJson` | Emite `debug_collision.json` (default ligado se nem `-DebugJson` nem `-BuildZsm` forem passados). |
| `-BuildZsm` | Emite `static_collision.zsm`. |
| `-RegionSize=` | Tamanho da célula em cm para `RegionId/GridX/GridY/GridZ`. Default `5000`. |
| `-AllowComplexAsSimple` | (futuro) permite triangle mesh estática como fallback. |
| `-Validate` | Não escreve ficheiros, apenas lista warnings. |

Códigos de saída:

- `0` — exportação com 1+ entidades.
- `1` — falta `-Map=`.
- `2` — pacote não pôde ser carregado.
- `3` — pacote sem `UWorld`.
- `4` — exportação correu mas não gerou nenhuma entidade (provavelmente falta de `UStaticMeshComponent` válidos).

## Saídas

- `<OutputDir>/debug_collision.json` — formato `zeus_debug_collision` v1, em centímetros, com `extentSemantics: "halfExtent"` para boxes.
- `<OutputDir>/static_collision.zsm` — formato binário `ZSMC` v1, little-endian, lido pelo `ZsmCollisionLoader` no servidor.

## Limitações desta fase

- `TriangleMesh` e `HeightField` não são exportados (reservados para Parte 4.5).
- World Partition cells não são iteradas explicitamente; usa-se `TActorIterator<AActor>` no `GWorld` actual.
- Os `RegionId/GridX/GridY/GridZ` são calculados a partir do centro dos `Bounds` (provisório até `ZeusRegionSystem`).
- Convex assume `VertexData` em local space; sem geração de hull se a `UStaticMesh` não trouxer `VertexData`.
