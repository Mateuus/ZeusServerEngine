# ZeusGame

Modulo de gameplay autoritativo do Zeus Server Engine. Agrupa subsistemas que
operam sobre `ZeusWorld` (`Actor`/`ActorComponent`) e `ZeusCollision`
(`CollisionWorld`).

## Estado atual (Parte 5)

Implementados:

- `Entities/CharacterActor` — capsula logica server-only com tags
  `Character` + `PlayerProxy`; regista um `MovementComponent` por defeito.
- `Movement/MovementComponent` — sweep & slide + step up + walkable floor +
  gravidade; depende apenas do `CollisionWorld`.
- `Movement/MovementSystem` — facade thin com `Configure(MovementSettings)`,
  ligacao opcional ao `CollisionWorld` e stats agregadas (TC-MOVE).
- `Movement/MovementTests` — TC-MOVE-001..010 programaticos contra um
  `BuildProgrammaticAsset()` enriquecido.

## Roadmap (subsistemas placeholder)

Cada pasta abaixo existe como placeholder e sera populada em fases futuras:

- `Combat/` — danos, hits e resistencias.
- `Spells/` — castbar, prefabs de feitico, projecteis.
- `Skills/` — talents, builds, cooldowns.
- `AuraStatus/` — buffs/debuffs persistentes.
- `Inventory/` — items, stacks, equipamento.
- `LagCompensation/` — re-simulation/rewind para hits autoritativos.
- `Rules/` — regras de partida (PvP, factions, limites).

Todas dependem de `Entities` + `Movement` ja entregues nesta fase.
