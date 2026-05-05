# Próximos passos — Zeus Server Engine (rede e além)

## Concluído nesta fase (Parte 2.5 — validação / hardening)

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

## Gameplay / motor (não iniciar antes da rede estar estável)

- World, Actor, Player, Character
- Movement, collision, replication, AOI
- Jolt, ZeusMapTools, inventário, combate, NPC, quest
- Plugin Unreal

## Ordem recomendada

1. **Parte 3 — World mínimo** (após checklist Parte 2.5 verificado no ambiente).
2. Parte 4 — Collision foundation  
3. Parte 5 — Movement foundation  
4. Parte 6 — Spawn de player  
5. Parte 7 — Replication/AOI  

## Documentação

- Expandir exemplos de payloads quando novos opcodes forem adicionados.
- Manter `Docs/NETWORK.md` e `Docs/NETWORK_TESTS.md` sincronizados com o wire real.
