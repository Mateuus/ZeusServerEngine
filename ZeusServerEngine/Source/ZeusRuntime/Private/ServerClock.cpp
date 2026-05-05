#include "ServerClock.hpp"

#include <algorithm>
#include <cmath>

namespace Zeus::Runtime
{
void ServerClock::Configure(double targetTps, int maxStepsPerFrame)
{
    targetTps_ = std::max(1.0, targetTps);
    maxStepsPerFrame_ = std::max(1, maxStepsPerFrame);
    fixedDeltaSeconds_ = 1.0 / targetTps_;
    accumulatorSeconds_ = 0.0;
}

int ServerClock::Advance(double realDeltaSeconds)
{
    constexpr double kMaxFrameSeconds = 0.25;
    const double clampedReal = std::min(std::max(0.0, realDeltaSeconds), kMaxFrameSeconds);

    stats_.TotalFrames += 1;
    stats_.LastFrameWallSeconds = clampedReal;
    stats_.MaxFrameWallSeconds = std::max(stats_.MaxFrameWallSeconds, clampedReal);
    stats_.AccumulatedFrameWallSeconds += clampedReal;

    accumulatorSeconds_ += clampedReal;

    int steps = 0;
    while (accumulatorSeconds_ >= fixedDeltaSeconds_ && steps < maxStepsPerFrame_)
    {
        accumulatorSeconds_ -= fixedDeltaSeconds_;
        ++steps;
        stats_.TotalFixedTicks += 1;
    }

    if (accumulatorSeconds_ >= fixedDeltaSeconds_ && steps == maxStepsPerFrame_)
    {
        // Anti-spiral: drop leftover accumulator after hitting the step cap.
        accumulatorSeconds_ = 0.0;
        stats_.SpiralClampEvents += 1;
    }

    return steps;
}

void ServerClock::ResetStats()
{
    stats_ = ServerClockStats{};
}
} // namespace Zeus::Runtime
