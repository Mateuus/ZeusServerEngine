#pragma once

#include "ZeusNonCopyable.hpp"
#include "ZeusTypes.hpp"

namespace Zeus::Runtime
{
struct ServerClockStats
{
    Zeus::u64 TotalFixedTicks = 0;
    Zeus::u64 TotalFrames = 0;
    double LastFrameWallSeconds = 0.0;
    double MaxFrameWallSeconds = 0.0;
    double AccumulatedFrameWallSeconds = 0.0;
    Zeus::u32 SpiralClampEvents = 0;
};

/**
 * Fixed timestep accumulator with anti-spiral (max steps per wall frame).
 * Real delta feeding Advance() must come from PlatformTime.
 */
class ServerClock : public Zeus::NonCopyable
{
public:
    void Configure(double targetTps, int maxStepsPerFrame);

    /** Wall time slice since last call (typically once per outer loop iteration). */
    int Advance(double realDeltaSeconds);

    [[nodiscard]] double GetFixedDeltaSeconds() const { return fixedDeltaSeconds_; }
    [[nodiscard]] double GetTargetTps() const { return targetTps_; }
    [[nodiscard]] const ServerClockStats& GetStats() const { return stats_; }

    void ResetStats();

private:
    double targetTps_ = 30.0;
    double fixedDeltaSeconds_ = 1.0 / 30.0;
    double accumulatorSeconds_ = 0.0;
    int maxStepsPerFrame_ = 5;
    ServerClockStats stats_;
};
} // namespace Zeus::Runtime
