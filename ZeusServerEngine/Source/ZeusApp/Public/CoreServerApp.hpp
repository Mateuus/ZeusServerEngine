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
/** Bootstrap for Part 1: config, logging, engine loop (no network). */
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
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace Zeus::App
