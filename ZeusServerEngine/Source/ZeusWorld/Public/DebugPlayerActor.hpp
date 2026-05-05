#pragma once

#include "Actor.hpp"
#include "Vector3.hpp"

#include <vector>

namespace Zeus::World
{
/**
 * Actor server-side usado para validar streaming de regioes sem clientes reais.
 * Percorre uma lista de waypoints em loop com lerp linear; ganha a tag
 * `PlayerProxy` em `OnSpawned` para que o `ZeusRegionSystem` o detecte.
 */
class DebugPlayerActor : public Actor
{
public:
    /**
     * Configura a rota e velocidade. Se for chamado depois de `OnSpawned`, a
     * posicao actual e mantida; o proximo segmento aponta para o waypoint
     * seguinte de forma estavel.
     */
    void SetWaypoints(std::vector<Zeus::Math::Vector3> waypoints, double speedCmS);

    void OnSpawned() override;
    void Tick(double deltaSeconds) override;

    std::size_t GetWaypointCount() const { return Waypoints.size(); }
    std::size_t GetCurrentSegment() const { return CurrentSegment; }
    double GetSpeedCmS() const { return SpeedCmS; }

private:
    std::vector<Zeus::Math::Vector3> Waypoints;
    double SpeedCmS = 0.0;
    std::size_t CurrentSegment = 0;
    double SegmentProgress = 0.0; // [0,1]
};
} // namespace Zeus::World
