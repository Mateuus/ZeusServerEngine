#include "CollisionTestScene.hpp"

#include "CollisionDebug.hpp"
#include "CollisionWorld.hpp"
#include "MathConstants.hpp"
#include "MathUtils.hpp"
#include "Quaternion.hpp"
#include "Rotator.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
CollisionShape MakeBox(double halfX, double halfY, double halfZ)
{
    CollisionShape Shape;
    Shape.Type = ECollisionShapeType::Box;
    Shape.Box.HalfExtents = Math::Vector3(halfX, halfY, halfZ);
    Shape.LocalTransform = Math::Transform::Identity;
    return Shape;
}

Math::Quaternion QuatFromYRoll(double pitchDegrees)
{
    const double half = pitchDegrees * 0.5 * Math::DegToRad;
    const double s = std::sin(half);
    const double c = std::cos(half);
    return Math::Quaternion(0.0, s, 0.0, c);
}

CollisionEntity MakeEntity(const std::string& name, const Math::Vector3& location,
    const Math::Quaternion& rotation, CollisionShape shape)
{
    CollisionEntity Entity;
    Entity.Name = name;
    Entity.ActorName = name + "_Actor";
    Entity.ComponentName = "StaticMeshComponent0";
    Entity.WorldTransform = Math::Transform(location, rotation, Math::Vector3::One);
    Entity.BoundsCenter = location;
    Entity.BoundsExtent = Math::Vector3(500.0, 500.0, 500.0);
    Entity.Region.RegionSizeCm = 5000.0;
    Entity.Shapes.push_back(std::move(shape));
    return Entity;
}

void RecordResult(TestSceneReport& Report, std::string name, bool bPassed, std::string detail = {})
{
    TestScenarioResult R;
    R.Name = std::move(name);
    R.bPassed = bPassed;
    R.Detail = std::move(detail);
    if (R.bPassed)
    {
        ++Report.PassedCount;
        CollisionDebug::LogInfo("CollisionTest",
            CollisionDebug::Concat("[CollisionTest] ", R.Name, ": OK ", R.Detail));
    }
    else
    {
        ++Report.FailedCount;
        CollisionDebug::LogWarn("CollisionTest",
            CollisionDebug::Concat("[CollisionTest] ", R.Name, ": FAIL ", R.Detail));
    }
    Report.Scenarios.push_back(std::move(R));
}
} // namespace

CollisionAsset CollisionTestScene::BuildProgrammaticAsset()
{
    CollisionAsset Asset;
    Asset.MapName = "TestWorld_Programmatic";
    Asset.Units = "centimeters";
    Asset.RegionSizeCm = 5000.0;
    Asset.Version = 1;
    Asset.Flags = 0;

    // Floor: 2000 x 2000 x 10 cm centred at origin (top of floor at z = +5).
    Asset.Entities.push_back(MakeEntity("Floor",
        Math::Vector3(0.0, 0.0, 0.0), Math::Quaternion::Identity,
        MakeBox(1000.0, 1000.0, 5.0)));

    // Wall: 50 x 500 x 300 cm at x = +500.
    Asset.Entities.push_back(MakeEntity("Wall",
        Math::Vector3(500.0, 0.0, 150.0), Math::Quaternion::Identity,
        MakeBox(25.0, 250.0, 150.0)));

    // Walkable ramp: 600 x 400 x 10 cm rotated 30° around Y at (-500, 200, 100).
    Asset.Entities.push_back(MakeEntity("RampValid",
        Math::Vector3(-500.0, 200.0, 100.0), QuatFromYRoll(30.0),
        MakeBox(300.0, 200.0, 5.0)));

    // Steep ramp: 400 x 400 x 10 cm rotated 70° around Y at (-500, -200, 200).
    Asset.Entities.push_back(MakeEntity("RampSteep",
        Math::Vector3(-500.0, -200.0, 200.0), QuatFromYRoll(70.0),
        MakeBox(200.0, 200.0, 5.0)));

    Asset.Stats.EntityCount = Asset.Entities.size();
    Asset.Stats.ShapeCount = Asset.Entities.size();
    Asset.Stats.BoxCount = Asset.Entities.size();
    return Asset;
}

TestSceneReport CollisionTestScene::RunAll(CollisionWorld& world)
{
    TestSceneReport Report;

    if (!world.IsPhysicsBuilt())
    {
        RecordResult(Report, "Pre-check: physics built", false,
            "(world.PhysicsBuilt = false)");
        return Report;
    }

    PhysicsWorld& Physics = world.GetPhysicsWorld();

    // --- TC-01 Capsule vs floor (RaycastDown finds the floor). ----------------
    {
        const Math::Vector3 Origin(0.0, 0.0, 200.0);
        RaycastHit Hit;
        const bool bHit = Physics.RaycastDown(Origin, 500.0, Hit);
        const bool bPass = bHit && Hit.Distance > 100.0 && Hit.Distance < 250.0
            && Math::IsWalkableFloor(Hit.ImpactNormal, Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(distance=" << Hit.Distance << ")";
        RecordResult(Report, "TC-01 Capsule vs floor (RaycastDown)", bPass, det.str());
    }

    // --- TC-02 Capsule overlaps wall. ----------------------------------------
    {
        const Math::Vector3 CapCenter(495.0, 0.0, 100.0);
        ContactInfo Contact;
        const bool bOverlap = Physics.CollideCapsule(CapCenter, 35.0, 90.0, Contact);
        RecordResult(Report, "TC-02 Capsule vs wall (CollideCapsule)", bOverlap);
    }

    // --- TC-03 Walkable ramp ~30°. -------------------------------------------
    {
        const Math::Vector3 Origin(-500.0, 200.0, 400.0);
        RaycastHit Hit;
        const bool bHit = Physics.RaycastDown(Origin, 800.0, Hit);
        const bool bWalkable = bHit && Math::IsWalkableFloor(Hit.ImpactNormal, Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(hit=" << bHit << " normal=" << Hit.ImpactNormal.ToString() << ")";
        RecordResult(Report, "TC-03 Walkable ramp (~30 deg)", bHit && bWalkable, det.str());
    }

    // --- TC-04 Steep ramp ~70°. ----------------------------------------------
    {
        const Math::Vector3 Origin(-500.0, -200.0, 600.0);
        RaycastHit Hit;
        const bool bHit = Physics.RaycastDown(Origin, 800.0, Hit);
        const bool bWalkable = bHit && Math::IsWalkableFloor(Hit.ImpactNormal, Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(hit=" << bHit << " walkable=" << bWalkable << ")";
        RecordResult(Report, "TC-04 Too steep ramp (~70 deg) blocks",
            bHit && !bWalkable, det.str());
    }

    // --- TC-05 Lateral raycast hits the wall. --------------------------------
    {
        const Math::Vector3 Origin(0.0, 0.0, 100.0);
        const Math::Vector3 Dir(1.0, 0.0, 0.0);
        RaycastHit Hit;
        const bool bHit = Physics.Raycast(Origin, Dir, 1000.0, Hit);
        const bool bPass = bHit && Hit.Distance > 400.0 && Hit.Distance < 600.0;
        std::ostringstream det;
        det << "(distance=" << Hit.Distance << ")";
        RecordResult(Report, "TC-05 Lateral raycast vs wall", bPass, det.str());
    }

    std::ostringstream summary;
    summary << "[CollisionTest] Summary passed=" << Report.PassedCount
            << " failed=" << Report.FailedCount;
    if (Report.FailedCount == 0)
    {
        CollisionDebug::LogInfo("CollisionTest", summary.str());
    }
    else
    {
        CollisionDebug::LogWarn("CollisionTest", summary.str());
    }
    return Report;
}
} // namespace Zeus::Collision
