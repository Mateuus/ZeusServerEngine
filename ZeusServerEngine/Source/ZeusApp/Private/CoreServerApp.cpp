#include "CoreServerApp.hpp"

#include "EngineLoop.hpp"
#include "MapInstance.hpp"
#include "WorldRuntime.hpp"
#include "NetworkDiagnostics.hpp"
#include "NetworkSimulator.hpp"
#include "NetConnectionManager.hpp"
#include "PacketParser.hpp"
#include "PacketStats.hpp"
#include "PlatformConsoleLive.hpp"
#include "PlatformTime.hpp"
#include "SessionManager.hpp"
#include "SessionNetworkSettings.hpp"
#include "SessionPacketHandler.hpp"
#include "SpawnParameters.hpp"
#include "UdpServer.hpp"
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

bool ParseJsonBoolValue(const std::string& json, const char* key, bool& outValue)
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
    size_t p = colon + 1;
    while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p])))
    {
        ++p;
    }
    if (p + 4 <= json.size() && json.compare(p, 4, "true") == 0)
    {
        outValue = true;
        return true;
    }
    if (p + 5 <= json.size() && json.compare(p, 5, "false") == 0)
    {
        outValue = false;
        return true;
    }
    return false;
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
    std::unique_ptr<Zeus::Net::UdpServer> udp;
    std::unique_ptr<Zeus::World::WorldRuntime> worldRuntime;
    Zeus::Net::NetConnectionManager netConnections;
    Zeus::Session::SessionManager sessions;
    Zeus::Session::SessionPacketHandler sessionPacketHandler;
    Zeus::Net::PacketStats netStats{};
    Zeus::Net::NetworkDiagnostics netDiag{};
    Zeus::Net::NetworkSimulator netSim{};
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

    impl_->sessionPacketHandler.SetPacketStats(&impl_->netStats);
    impl_->sessionPacketHandler.SetNetworkDiagnostics(&impl_->netDiag);

    Zeus::Session::SessionNetworkSettings netSettings{};
    {
        int connectionTimeoutMs = 30000;
        if (ParseJsonIntValue(json, "ConnectionTimeoutMs", connectionTimeoutMs))
        {
            netSettings.connectionTimeoutMs = std::max(1000, connectionTimeoutMs);
        }
        bool networkDebugAck = false;
        if (ParseJsonBoolValue(json, "NetworkDebugAck", networkDebugAck))
        {
            netSettings.networkDebugAck = networkDebugAck;
        }
        int reliableResendIntervalMs = 250;
        if (ParseJsonIntValue(json, "ReliableResendIntervalMs", reliableResendIntervalMs))
        {
            netSettings.reliableResendIntervalSec =
                static_cast<double>(std::max(10, reliableResendIntervalMs)) / 1000.0;
        }
        int reliableMaxResends = 12;
        if (ParseJsonIntValue(json, "ReliableMaxResends", reliableMaxResends))
        {
            netSettings.reliableMaxResends =
                static_cast<std::uint32_t>(std::clamp(reliableMaxResends, 0, 100));
        }
        int maxLoadingFragmentCount = 4096;
        if (ParseJsonIntValue(json, "MaxLoadingFragmentCount", maxLoadingFragmentCount))
        {
            netSettings.maxLoadingFragmentCount =
                static_cast<std::uint32_t>(std::max(1, maxLoadingFragmentCount));
        }
        int maxReassemblyBytes = static_cast<int>(netSettings.maxReassemblyBytes);
        if (ParseJsonIntValue(json, "MaxReassemblyBytes", maxReassemblyBytes))
        {
            netSettings.maxReassemblyBytes =
                static_cast<std::uint32_t>(std::max(1024, maxReassemblyBytes));
        }
        int reassemblyTimeoutMs = 60000;
        if (ParseJsonIntValue(json, "ReassemblyTimeoutMs", reassemblyTimeoutMs))
        {
            netSettings.reassemblyTimeoutMs = std::max(1000, reassemblyTimeoutMs);
        }
        int maxOrderedPendingPerChannel = 64;
        if (ParseJsonIntValue(json, "MaxOrderedPendingPerChannel", maxOrderedPendingPerChannel))
        {
            netSettings.maxOrderedPendingPerChannel =
                static_cast<std::uint32_t>(std::max(1, maxOrderedPendingPerChannel));
        }
        int maxOrderedGap = 128;
        if (ParseJsonIntValue(json, "MaxOrderedGap", maxOrderedGap))
        {
            netSettings.maxOrderedGap = static_cast<std::uint32_t>(std::max(1, maxOrderedGap));
        }
        impl_->sessionPacketHandler.Configure(netSettings);
        impl_->netConnections.SetReliabilitySendPolicy(
            netSettings.reliableResendIntervalSec, netSettings.reliableMaxResends);
        impl_->netConnections.SetOrderedInboundPolicy(
            netSettings.maxOrderedPendingPerChannel, netSettings.maxOrderedGap);
    }

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

    int listenUdp = 0;
    (void)ParseJsonIntValue(json, "ListenUdpPort", listenUdp);
    int netSimDropPermille = 0;
    (void)ParseJsonIntValue(json, "NetSimDropPermille", netSimDropPermille);
    if (netSimDropPermille < 0)
    {
        netSimDropPermille = 0;
    }
    if (netSimDropPermille > 1000)
    {
        netSimDropPermille = 1000;
    }
    impl_->netSim.Configure(
        static_cast<std::uint16_t>(netSimDropPermille),
        static_cast<std::uint32_t>(listenUdp > 0 ? listenUdp : 1));

    if (listenUdp > 0 && listenUdp <= 65535)
    {
        impl_->udp = std::make_unique<Zeus::Net::UdpServer>();
        const ZeusResult udpStart = impl_->udp->Start(static_cast<std::uint16_t>(listenUdp));
        if (!udpStart.Ok())
        {
            ZeusLog::Warning("Net", std::string_view(udpStart.GetErrorText()));
            impl_->udp.reset();
        }
        else
        {
            const std::string listenLine = std::string("UDP listening on 0.0.0.0:") + std::to_string(listenUdp) + " (clients can connect)";
            ZeusLog::Info("Net", std::string_view(listenLine));
        }
    }
    else if (listenUdp != 0)
    {
        ZeusLog::Warning(
            "Net",
            "ListenUdpPort must be between 1 and 65535. UDP disabled; fix Config/server.json and restart.");
    }
    else
    {
        ZeusLog::Info(
            "Net",
            "UDP disabled (ListenUdpPort=0). Set ListenUdpPort to 1-65535 in Config/server.json to listen for clients.");
    }

    impl_->worldRuntime = std::make_unique<Zeus::World::WorldRuntime>();
    (void)impl_->worldRuntime->Initialize();

    std::string defaultMap = "TestWorld";
    (void)ParseJsonStringValue(json, "WorldDefaultMap", defaultMap);

    Zeus::World::MapInstance* mainMap = impl_->worldRuntime->CreateMapInstance(defaultMap);
    if (mainMap != nullptr)
    {
        mainMap->BeginPlay();
    }

    bool worldDebugSpawnActors = false;
    (void)ParseJsonBoolValue(json, "WorldDebugSpawnActors", worldDebugSpawnActors);
    if (worldDebugSpawnActors && mainMap != nullptr)
    {
        Zeus::World::World& w = mainMap->GetWorld();
        static constexpr const char* kDebugNames[] = {"DebugActor_01", "DebugActor_02", "DebugActor_03"};
        for (const char* name : kDebugNames)
        {
            Zeus::World::SpawnParameters params;
            params.Name = name;
            w.SpawnActor(params);
        }
    }

    impl_->loop->SetFixedTickCallback([this](const double fixedDeltaSeconds) { RunFixedTick(fixedDeltaSeconds); });

    ZeusLog::Info("App", "CoreServerApp initialized (Part 1 + Part 2 network + Part 3 world runtime).");
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

    if (impl_->loop)
    {
        impl_->loop->SetFixedTickCallback({});
    }
    if (impl_->worldRuntime)
    {
        impl_->worldRuntime->Shutdown();
        impl_->worldRuntime.reset();
    }
    if (impl_->udp)
    {
        impl_->udp->Stop();
        impl_->udp.reset();
    }
    impl_->sessions.Clear();
    impl_->netConnections.Clear();

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

void CoreServerApp::RunFixedTick(const double fixedDeltaSeconds)
{
    (void)fixedDeltaSeconds;
    const double nowMono = Zeus::Platform::NowMonotonicSeconds();
    const std::uint64_t wallMs = Zeus::Platform::NowUnixEpochMilliseconds();

    if (impl_->udp)
    {
        impl_->udp->PumpReceive([this, nowMono, wallMs](const std::uint8_t* data, const std::size_t n, const Zeus::Net::UdpEndpoint& from) {
            if (impl_->netSim.ShouldDropInbound())
            {
                ++impl_->netStats.DatagramsSimDropped;
                return;
            }
            Zeus::Protocol::PacketParser::Output parsed{};
            const ZeusResult pr = Zeus::Protocol::PacketParser::Parse(data, n, false, parsed);
            if (!pr.Ok())
            {
                ++impl_->netStats.DatagramsRejectedParse;
                return;
            }
            impl_->sessionPacketHandler.OnDatagram(
                *impl_->udp,
                impl_->netConnections,
                impl_->sessions,
                nowMono,
                wallMs,
                parsed,
                from);
        });
        impl_->sessionPacketHandler.OnTickPostNetwork(
            *impl_->udp, impl_->netConnections, impl_->sessions, nowMono, wallMs);
    }

    impl_->sessionPacketHandler.OnTickTimeouts(impl_->netConnections, impl_->sessions, nowMono);

    if (impl_->worldRuntime)
    {
        impl_->worldRuntime->Tick(fixedDeltaSeconds);
    }
}
} // namespace Zeus::App
