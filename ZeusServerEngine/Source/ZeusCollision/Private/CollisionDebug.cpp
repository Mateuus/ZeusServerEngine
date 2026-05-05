#include "CollisionDebug.hpp"

#include "ZeusLog.hpp"

#include <sstream>
#include <string>

namespace Zeus::Collision::CollisionDebug
{
void LogInfo(std::string_view category, std::string_view message)
{
    Zeus::ZeusLog::Info(category, message);
}

void LogWarn(std::string_view category, std::string_view message)
{
    Zeus::ZeusLog::Warning(category, message);
}

void LogError(std::string_view category, std::string_view message)
{
    Zeus::ZeusLog::Error(category, message);
}

std::string Concat(std::string_view a, std::string_view b)
{
    std::string out;
    out.reserve(a.size() + b.size());
    out.append(a);
    out.append(b);
    return out;
}

std::string Concat(std::string_view a, std::string_view b, std::string_view c)
{
    std::string out;
    out.reserve(a.size() + b.size() + c.size());
    out.append(a);
    out.append(b);
    out.append(c);
    return out;
}

std::string Concat(std::string_view a, std::string_view b, std::string_view c, std::string_view d)
{
    std::string out;
    out.reserve(a.size() + b.size() + c.size() + d.size());
    out.append(a);
    out.append(b);
    out.append(c);
    out.append(d);
    return out;
}

void LogAssetSummary(const CollisionAsset& asset, std::string_view sourcePath)
{
    std::ostringstream oss;
    oss << "Loaded entities=" << asset.Stats.EntityCount
        << " shapes=" << asset.Stats.ShapeCount
        << " (box=" << asset.Stats.BoxCount
        << " sphere=" << asset.Stats.SphereCount
        << " capsule=" << asset.Stats.CapsuleCount
        << " convex=" << asset.Stats.ConvexCount
        << ") map=" << asset.MapName
        << " source=" << sourcePath;
    Zeus::ZeusLog::Info("Collision", oss.str());
}
} // namespace Zeus::Collision::CollisionDebug
