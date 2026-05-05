#pragma once

#include "ServerClock.hpp"
#include "ZeusNonCopyable.hpp"
#include "ZeusTypes.hpp"

#include <atomic>
#include <functional>

namespace Zeus::Runtime
{
/** Owns ServerClock and drives the main fixed-tick loop (no network/world yet). */
class EngineLoop : public Zeus::NonCopyable
{
public:
    void Configure(double targetTps, int maxStepsPerFrame);

    /**
     * Opcional: ponteiro para flag escrita apenas por async-signal-safe handlers (ex. SIGINT no Linux).
     * O loop termina quando RequestStop() ou *externalStop fica true.
     */
    void SetExternalStopFlag(std::atomic<bool>* externalStop);

    /** Opcional: chamado no final de cada tick fixo (ex.: `ZeusNet::UdpServer::PumpReceive`). */
    void SetFixedTickCallback(std::function<void(double fixedDeltaSeconds)> callback);

    /** Blocks until RequestStop(); processes fixed ticks and optional idle sleep. */
    void Run();

    void RequestStop();
    [[nodiscard]] bool IsStopRequested() const { return stopRequested_.load(std::memory_order_relaxed); }

    [[nodiscard]] ServerClock& GetClock() { return clock_; }
    [[nodiscard]] const ServerClock& GetClock() const { return clock_; }

private:
    void FixedUpdate(double fixedDeltaSeconds);

    ServerClock clock_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool>* externalStop_ = nullptr;
    Zeus::u64 stubTickCounter_ = 0;
    std::function<void(double)> fixedTickCallback_;
};
} // namespace Zeus::Runtime
