#pragma once

#include "ZeusResult.hpp"

#include <filesystem>
#include <memory>

namespace Zeus::Runtime
{
class EngineLoop;
}

namespace Zeus::App
{
/** Bootstrap: Parte 1 (runtime) + Parte 2 (UDP opcional via `ListenUdpPort`). */
class CoreServerApp
{
public:
    CoreServerApp();
    ~CoreServerApp();

    CoreServerApp(const CoreServerApp&) = delete;
    CoreServerApp& operator=(const CoreServerApp&) = delete;

    /** Loads Config/server.json relative to cwd or optional base path. */
    ZeusResult Initialize(const std::filesystem::path& configPath);

    /** Blocks until shutdown (Ctrl+C or future API). */
    void Run();

    void Shutdown();

    void RequestStop();

private:
    void RunFixedTick(double fixedDeltaSeconds);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace Zeus::App
