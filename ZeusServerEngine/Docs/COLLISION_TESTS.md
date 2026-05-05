# COLLISION_TESTS — bateria de smoke tests da Parte 4

Os smoke tests são determinísticos e correm dentro do servidor quando `Collision.RunCollisionSmokeTest = true` em `Config/server.json`. Não dependem de ficheiros externos: usam o asset programático criado em `CollisionTestScene::BuildProgrammaticAsset()`.

| ID | Cenário | Verifica |
|----|---------|----------|
| **TEST-COLLISION-001** | Asset programático carrega no `CollisionWorld`. | `LoadFromAsset` devolve `true` e `EntityCount=4`. |
| **TEST-COLLISION-002** | `BuildPhysicsWorld` cria todos os bodies estáticos. | Log `Static bodies created=N skipped=0`. |
| **TEST-COLLISION-003 (TC-01)** | Capsule cai sobre o chão. | `RaycastDown` em `(0, 0, 200)` retorna `bHit=true` e `distance ∈ [100, 250]` cm. |
| **TEST-COLLISION-004 (TC-02)** | Capsule sobreposta à parede. | `CollideCapsule(center=(495, 0, 100), r=35, hh=90)` retorna `true`. |
| **TEST-COLLISION-005 (TC-03)** | Rampa válida ~30°. | `RaycastDown` na rampa retorna normal cuja `IsWalkableFloor(normal, 45)` = `true`. |
| **TEST-COLLISION-006 (TC-04)** | Rampa íngreme ~70°. | `RaycastDown` retorna normal cuja `IsWalkableFloor(normal, 45)` = `false`. |
| **TEST-COLLISION-007 (TC-05)** | Raycast lateral acerta a parede. | `Raycast` em `(0, 0, 100)` direção `+X` 1000 cm retorna `bHit=true` e `distance ∈ [400, 600]` cm. |
| **TEST-COLLISION-008** | ZSM round-trip campo-a-campo. | Após `BinaryWriter::Write` + `ZsmCollisionLoader::LoadFromBytes`, `EntityCount/ShapeCount/MapName` batem. (Ainda não automatizado — manual: `ZeusMapTools` → server load.) |
| **TEST-COLLISION-009** | Convex hull factory aceita ≥4 vértices. | `JoltShapeFactory::CreateShape` devolve shape válido para 8 vértices; falha para 3 (warning). |
| **TEST-COLLISION-010** | Shutdown limpo. | `CollisionWorld::Shutdown()` remove todos bodies, `JPH::Factory` é restaurada e o servidor pára sem leaks. |

## Como correr

`server-collision-smoke.example.json` está em `Config/`. Para correr o smoke:

```
ZeusServer.exe Config\server-collision-smoke.example.json
```

Quando o servidor arranca com `EnableCollisionWorld=true` e `RunCollisionSmokeTest=true`, mas o `static_collision.zsm` ainda não existe, ele cai no asset programático e corre TC-01..TC-05. Se o ficheiro existir, é ele que é usado para os bodies, e os smoke tests assumem que o asset programático é equivalente. Para testar com o asset real exportado pela Unreal, basta apontar `CollisionFile` para o `.zsm` produzido pelo `ZeusMapTools`.

## Critérios de aceitação

- `[CollisionTest] Summary passed=5 failed=0` no log do arranque.
- `Static bodies created` ≥ 4 (chão + parede + rampa válida + rampa íngreme).
- Shutdown sem warnings vermelhos (`Jolt assert`, `Could not destroy body`, `Factory leak`).
