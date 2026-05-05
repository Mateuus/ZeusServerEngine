#include "EngineLoop.hpp"

#include "PlatformConsole.hpp"
#include "PlatformTime.hpp"
#include "ZeusLog.hpp"

#include <chrono>
#include <cmath>
#include <string>
#include <thread>

namespace Zeus::Runtime
{
void EngineLoop::Configure(double targetTps, int maxStepsPerFrame)
{
    clock_.Configure(targetTps, maxStepsPerFrame);
    stopRequested_.store(false, std::memory_order_relaxed);
    stubTickCounter_ = 0;
}

void EngineLoop::SetExternalStopFlag(std::atomic<bool>* externalStop)
{
    externalStop_ = externalStop;
}

void EngineLoop::Run()
{
    Zeus::Platform::SetConsoleTitleUtf8("Zeus Server | iniciando...");

    double lastWall = Zeus::Platform::NowMonotonicSeconds();
    double statsIntervalStart = lastWall;
    Zeus::u64 fixedTicksThisInterval = 0;

    while (!stopRequested_.load(std::memory_order_relaxed) &&
           !(externalStop_ != nullptr && externalStop_->load(std::memory_order_relaxed)))
    {
        const double wallStart = Zeus::Platform::NowMonotonicSeconds();
        const double realDt = wallStart - lastWall;
        lastWall = wallStart;

        const int steps = clock_.Advance(realDt);
        for (int i = 0; i < steps; ++i)
        {
            FixedUpdate(clock_.GetFixedDeltaSeconds());
            fixedTicksThisInterval += 1;
        }

        const double wallNow = Zeus::Platform::NowMonotonicSeconds();
        if (wallNow - statsIntervalStart >= 1.0)
        {
            const double interval = wallNow - statsIntervalStart;
            const double tps = interval > 0.0 ? static_cast<double>(fixedTicksThisInterval) / interval : 0.0;
            const std::string title = std::string("Zeus Server | TPS ") + std::to_string(tps) + " | ticks/s " +
                                      std::to_string(fixedTicksThisInterval) + " | total " +
                                      std::to_string(stubTickCounter_);
            Zeus::Platform::SetConsoleTitleUtf8(title);
            fixedTicksThisInterval = 0;
            statsIntervalStart = wallNow;
        }

        const double wallUsed = Zeus::Platform::NowMonotonicSeconds() - wallStart;
        const double budget = 1.0 / clock_.GetTargetTps();
        if (wallUsed < budget)
        {
            const double sleepSeconds = budget - wallUsed;
            auto sleepNs = static_cast<std::chrono::nanoseconds::rep>(sleepSeconds * 1e9);
            if (sleepNs > 0)
            {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleepNs));
            }
        }
    }

    Zeus::Platform::SetConsoleTitleUtf8("Zeus Server | parado");
    ZeusLog::Info("Runtime", "EngineLoop::Run exiting (stop requested).");
}

void EngineLoop::RequestStop()
{
    stopRequested_.store(true, std::memory_order_relaxed);
}

void EngineLoop::FixedUpdate(double fixedDeltaSeconds)
{
    (void)fixedDeltaSeconds;
    ++stubTickCounter_;
}
} // namespace Zeus::Runtime
