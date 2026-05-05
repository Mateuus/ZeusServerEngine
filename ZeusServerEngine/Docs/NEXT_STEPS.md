# Próximos passos — Zeus Server Engine (rede e além)

## Rede / transporte (melhorias incrementais)

- Política de `S_DISCONNECT_OK` automático ao expirar timeout (sem depender só de remoção silenciosa).
- Timeout e intervalos configuráveis por JSON (além dos valores fixos atuais).
- Testes automatizados (CI) para wire + ClientTest e cenários de perda (NetSim).
- `NetReceiveQueue` explícita se o motor precisar de priorização na receção.

## Gameplay / motor (não iniciar antes da rede estar estável)

- World, Actor, Player, Character
- Movement, collision, replication, AOI
- Jolt, ZeusMapTools, inventário, combate, NPC, quest
- Plugin Unreal

## Documentação

- Expandir exemplos de payloads quando novos opcodes forem adicionados
