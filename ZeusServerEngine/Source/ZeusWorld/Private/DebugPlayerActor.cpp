#include "DebugPlayerActor.hpp"

#include "Vector3.hpp"

#include <algorithm>

namespace Zeus::World
{
void DebugPlayerActor::SetWaypoints(std::vector<Zeus::Math::Vector3> waypoints, double speedCmS)
{
    Waypoints = std::move(waypoints);
    SpeedCmS = std::max(0.0, speedCmS);
    CurrentSegment = 0;
    SegmentProgress = 0.0;
    if (!Waypoints.empty())
    {
        SetLocation(Waypoints[0]);
    }
}

void DebugPlayerActor::OnSpawned()
{
    Actor::OnSpawned();
    AddTag("PlayerProxy");
}

void DebugPlayerActor::Tick(double deltaSeconds)
{
    Actor::Tick(deltaSeconds);

    if (Waypoints.size() < 2 || SpeedCmS <= 0.0 || deltaSeconds <= 0.0)
    {
        return;
    }

    const std::size_t total = Waypoints.size();
    const Zeus::Math::Vector3& A = Waypoints[CurrentSegment % total];
    const Zeus::Math::Vector3& B = Waypoints[(CurrentSegment + 1) % total];
    const double segLen = Zeus::Math::Vector3::Distance(A, B);
    if (segLen <= 1e-6)
    {
        CurrentSegment = (CurrentSegment + 1) % total;
        SegmentProgress = 0.0;
        return;
    }

    const double advanceCm = SpeedCmS * deltaSeconds;
    SegmentProgress += advanceCm / segLen;
    while (SegmentProgress >= 1.0)
    {
        SegmentProgress -= 1.0;
        CurrentSegment = (CurrentSegment + 1) % total;
    }

    const Zeus::Math::Vector3& From = Waypoints[CurrentSegment % total];
    const Zeus::Math::Vector3& To = Waypoints[(CurrentSegment + 1) % total];
    const double t = std::clamp(SegmentProgress, 0.0, 1.0);
    const Zeus::Math::Vector3 pos = From + (To - From) * t;
    SetLocation(pos);
}
} // namespace Zeus::World
