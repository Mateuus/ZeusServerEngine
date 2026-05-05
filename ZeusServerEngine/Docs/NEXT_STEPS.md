# Próximos passos — Zeus Server Engine (rede e além)

Itens **intencionalmente** fora do âmbito da etapa atual (handshake + ping/pong + disconnect):

## Rede / transporte

- Fila de envio e **resend** para `Reliable` / `ReliableOrdered` de verdade
- Ordenação estrita e dedupe completo em todos os canais
- Timeout configurável por JSON, política de `S_DISCONNECT_OK` automático ao expirar
- Métricas (`NetStats`), perda, RTT agregado no servidor
- `NetSendQueue` / `NetReceiveQueue` conforme plano longo

## Gameplay / motor (não iniciar antes da rede estar estável)

- World, Actor, Player, Character
- Movement, collision, replication, AOI
- Jolt, ZeusMapTools, inventário, combate, NPC, quest
- Plugin Unreal

## Documentação

- Expandir exemplos de payloads quando novos opcodes forem adicionados
- Testes automatizados (CI) para wire + ClientTest
