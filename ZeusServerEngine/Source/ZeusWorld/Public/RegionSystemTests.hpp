#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Zeus::World
{
struct RegionTestResult
{
    std::string Name;
    bool bPassed = false;
    std::string Detail;
};

struct RegionTestReport
{
    std::vector<RegionTestResult> Scenarios;
    std::size_t PassedCount = 0;
    std::size_t FailedCount = 0;
};

/**
 * Smoke tests programaticos do `ZeusRegionSystem`. Constroem um asset
 * sintetico com 9 entidades em 9 cells diferentes e validam o
 * load/unload conforme o `DebugPlayerActor` se move.
 */
class RegionSystemTests
{
public:
    static RegionTestReport RunAll();
};
} // namespace Zeus::World
