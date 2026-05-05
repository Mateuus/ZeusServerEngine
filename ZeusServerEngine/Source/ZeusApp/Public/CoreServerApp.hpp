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

    /**
     * Carrega o JSON de configuração. Caminhos relativos em `server.json` (Data, Logs, …)
     * resolvem-se contra `contentRoot` (raiz típica: pasta com `Config/` e `Data/`).
     */
    ZeusResult Initialize(const std::filesystem::path& configPath,
        const std::filesystem::path& contentRoot);

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
