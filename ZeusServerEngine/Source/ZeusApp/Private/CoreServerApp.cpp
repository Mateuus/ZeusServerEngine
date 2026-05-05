#include "CoreServerApp.hpp"

#include "EngineLoop.hpp"
#include "PlatformConsoleLive.hpp"
#include "ZeusLog.hpp"

#include <atomic>

#if !defined(_WIN32)
#include <csignal>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <filesystem>

namespace Zeus::App
{
namespace
{
#if defined(_WIN32)
CoreServerApp* g_consoleHandlerApp = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT)
    {
        if (g_consoleHandlerApp != nullptr)
        {
            g_consoleHandlerApp->RequestStop();
        }
        return TRUE;
    }
    return FALSE;
}
#endif

#if !defined(_WIN32)
std::atomic<bool>* g_posixStopTarget = nullptr;

extern "C" void ZeusPosixStopHandler(int /*sig*/)
{
    std::atomic<bool>* const t = g_posixStopTarget;
    if (t != nullptr)
    {
        t->store(true, std::memory_order_relaxed);
    }
}
#endif

std::string ReadEntireFile(const std::filesystem::path& path, ZeusResult& err)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        err = ZeusResult::Failure(ZeusErrorCode::IoError, "Failed to open config file.");
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in && !in.eof())
    {
        err = ZeusResult::Failure(ZeusErrorCode::IoError, "Failed to read config file.");
        return {};
    }
    err = ZeusResult::Success();
    return ss.str();
}

bool ParseJsonIntValue(const std::string& json, const char* key, int& outValue)
{
    const std::string quotedKey = std::string("\"") + key + "\"";
    const size_t k = json.find(quotedKey);
    if (k == std::string::npos)
    {
        return false;
    }
    size_t colon = json.find(':', k + quotedKey.size());
    if (colon == std::string::npos)
    {
        return false;
    }
    ++colon;
    while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon])))
    {
        ++colon;
    }
    size_t end = colon;
    if (colon < json.size() && (json[colon] == '-' || json[colon] == '+'))
    {
        ++end;
    }
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])))
    {
        ++end;
    }
    if (end == colon)
    {
        return false;
    }
    outValue = std::stoi(json.substr(colon, end - colon));
    return true;
}

bool ParseJsonStringValue(const std::string& json, const char* key, std::string& outValue)
{
    const std::string quotedKey = std::string("\"") + key + "\"";
    const size_t k = json.find(quotedKey);
    if (k == std::string::npos)
    {
        return false;
    }
    const size_t colon = json.find(':', k + quotedKey.size());
    if (colon == std::string::npos)
    {
        return false;
    }
    const size_t q0 = json.find('"', colon + 1);
    if (q0 == std::string::npos)
    {
        return false;
    }
    const size_t q1 = json.find('"', q0 + 1);
    if (q1 == std::string::npos)
    {
        return false;
    }
    outValue = json.substr(q0 + 1, q1 - q0 - 1);
    return true;
}

LogLevel ParseLogLevelString(const std::string& s)
{
    if (s == "Trace")
    {
        return LogLevel::Trace;
    }
    if (s == "Debug")
    {
        return LogLevel::Debug;
    }
    if (s == "Warning" || s == "Warn")
    {
        return LogLevel::Warning;
    }
    if (s == "Error")
    {
        return LogLevel::Error;
    }
    return LogLevel::Info;
}
} // namespace

struct CoreServerApp::Impl
{
    std::unique_ptr<Zeus::Runtime::EngineLoop> loop = std::make_unique<Zeus::Runtime::EngineLoop>();
    int targetTps = 30;
    bool initialized = false;
    bool shutDownDone = false;
#if !defined(_WIN32)
    std::atomic<bool> posixInterrupt{false};
    bool posixSignalsRegistered = false;
    struct sigaction previousSigint {};
    struct sigaction previousSigterm {};
#endif
};

CoreServerApp::CoreServerApp()
    : impl_(std::make_unique<Impl>())
{
}

CoreServerApp::~CoreServerApp()
{
    Shutdown();
}

ZeusResult CoreServerApp::Initialize(const std::filesystem::path& configPath)
{
    ZeusResult readErr = ZeusResult::Success();
    const std::string json = ReadEntireFile(configPath, readErr);
    if (!readErr.Ok())
    {
        return readErr;
    }

    int tps = impl_->targetTps;
    if (!ParseJsonIntValue(json, "TargetTPS", tps))
    {
        tps = 30;
    }
    impl_->targetTps = std::max(1, tps);

    std::string levelStr;
    if (ParseJsonStringValue(json, "LogLevel", levelStr))
    {
        ZeusLog::SetMinimumLevel(ParseLogLevelString(levelStr));
    }
    else
    {
        ZeusLog::SetMinimumLevel(LogLevel::Info);
    }

    std::string sessionLogDirStr = "Logs";
    (void)ParseJsonStringValue(json, "SessionLogDir", sessionLogDirStr);
    const std::filesystem::path sessionLogDir(sessionLogDirStr);
    const ZeusResult sessionLogResult = ZeusLog::OpenSessionLog(sessionLogDir);
    if (!sessionLogResult.Ok())
    {
        ZeusLog::Warning("App", std::string_view(sessionLogResult.GetErrorText()));
    }

    int overlayRows = 0;
    if (!ParseJsonIntValue(json, "DebugOverlayRows", overlayRows))
    {
        overlayRows = 0;
    }
    overlayRows = std::max(0, overlayRows);

    impl_->loop->Configure(static_cast<double>(impl_->targetTps), 5);
    impl_->initialized = true;
    impl_->shutDownDone = false;

#if defined(_WIN32)
    g_consoleHandlerApp = this;
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    impl_->loop->SetExternalStopFlag(nullptr);
#else
    impl_->posixInterrupt.store(false, std::memory_order_relaxed);
    impl_->loop->SetExternalStopFlag(&impl_->posixInterrupt);

    struct sigaction sa {};
    sa.sa_handler = ZeusPosixStopHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, &impl_->previousSigint) != 0 || sigaction(SIGTERM, &sa, &impl_->previousSigterm) != 0)
    {
        impl_->loop->SetExternalStopFlag(nullptr);
        ZeusLog::Warning("App", "sigaction(SIGINT/SIGTERM) failed; use kill -9 to stop if needed.");
    }
    else
    {
        g_posixStopTarget = &impl_->posixInterrupt;
        impl_->posixSignalsRegistered = true;
    }
#endif

    const ZeusResult panelResult = Zeus::Platform::ConsoleLivePanel::Init(static_cast<std::uint32_t>(overlayRows));
    if (!panelResult.Ok())
    {
        ZeusLog::Warning("App", std::string_view(panelResult.GetErrorText()));
    }
    else if (overlayRows > 0)
    {
        Zeus::Platform::ConsoleLivePanel::SetHeader("LIVE DEBUG (slots = DebugOverlayRows)");
        Zeus::Platform::ConsoleLivePanel::SetSlot(0, "Zeus | reserved for player/replication debug");
    }

    ZeusLog::Info("App", "CoreServerApp initialized (Part 1 foundation).");
    const std::string tpsLine = "TargetTPS=" + std::to_string(impl_->targetTps);
    ZeusLog::Info("App", std::string_view(tpsLine));
    if (ZeusLog::IsSessionLogOpen())
    {
        const std::string logPath = ZeusLog::GetSessionLogPath().string();
        ZeusLog::Info("App", std::string_view(std::string("Session log: ").append(logPath)));
    }
    return ZeusResult::Success();
}

void CoreServerApp::Run()
{
    if (!impl_->initialized)
    {
        ZeusLog::Error("App", "Initialize() was not called or failed.");
        return;
    }
    ZeusLog::Info("Runtime", "Starting EngineLoop (Ctrl+C to stop).");
    impl_->loop->Run();
}

void CoreServerApp::Shutdown()
{
    if (!impl_ || impl_->shutDownDone)
    {
        return;
    }
    impl_->shutDownDone = true;

#if defined(_WIN32)
    if (g_consoleHandlerApp == this)
    {
        SetConsoleCtrlHandler(nullptr, FALSE);
        g_consoleHandlerApp = nullptr;
    }
#else
    if (impl_->posixSignalsRegistered)
    {
        (void)sigaction(SIGINT, &impl_->previousSigint, nullptr);
        (void)sigaction(SIGTERM, &impl_->previousSigterm, nullptr);
        g_posixStopTarget = nullptr;
        impl_->posixSignalsRegistered = false;
    }
#endif

    Zeus::Platform::ConsoleLivePanel::Shutdown();

    if (impl_->loop)
    {
        impl_->loop->SetExternalStopFlag(nullptr);
    }
    impl_->loop.reset();
    impl_->initialized = false;
    ZeusLog::Info("App", "CoreServerApp shutdown complete.");
    ZeusLog::CloseSessionLog();
}

void CoreServerApp::RequestStop()
{
    if (impl_ && impl_->loop)
    {
        impl_->loop->RequestStop();
    }
}
} // namespace Zeus::App
