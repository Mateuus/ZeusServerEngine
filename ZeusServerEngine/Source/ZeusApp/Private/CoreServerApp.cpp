#include "CoreServerApp.hpp"
#include "ServerPaths.hpp"

#include "CollisionTestScene.hpp"
#include "CollisionWorld.hpp"
#include "DebugPlayerActor.hpp"
#include "DynamicCollisionWorld.hpp"
#include "Entities/CharacterActor.hpp"
#include "Movement/MovementComponent.hpp"
#include "Movement/MovementSystem.hpp"
#include "Movement/MovementTests.hpp"
#include "RegionSystemTests.hpp"
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
#include "Vector3.hpp"
#include "ZeusLog.hpp"
#include "ZeusRegionSystem.hpp"

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
#include <array>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

/**
 * Lê um objecto JSON plano `"keyName": { "k1": "v1", "k2": "v2" }` sem nesting.
 * Usado para `Collision.CollisionFileByMap` (nome lógico do mapa → caminho .zsm).
 */
bool ParseJsonFlatStringObject(const std::string& json, const char* keyName, std::unordered_map<std::string, std::string>& out)
{
    out.clear();
    const std::string quotedKey = std::string("\"") + keyName + "\"";
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
    size_t brace = json.find('{', colon);
    if (brace == std::string::npos)
    {
        return false;
    }
    size_t pos = brace + 1;
    while (pos < json.size())
    {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
        {
            ++pos;
        }
        if (pos >= json.size())
        {
            break;
        }
        if (json[pos] == '}')
        {
            ++pos;
            break;
        }
        if (json[pos] == ',')
        {
            ++pos;
            continue;
        }
        if (json[pos] != '"')
        {
            return false;
        }
        const size_t keyStart = pos + 1;
        const size_t keyEnd = json.find('"', keyStart);
        if (keyEnd == std::string::npos)
        {
            return false;
        }
        const std::string entryKey = json.substr(keyStart, keyEnd - keyStart);
        pos = keyEnd + 1;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
        {
            ++pos;
        }
        if (pos >= json.size() || json[pos] != ':')
        {
            return false;
        }
        ++pos;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
        {
            ++pos;
        }
        if (pos >= json.size() || json[pos] != '"')
        {
            return false;
        }
        const size_t valStart = pos + 1;
        const size_t valEnd = json.find('"', valStart);
        if (valEnd == std::string::npos)
        {
            return false;
        }
        out[entryKey] = json.substr(valStart, valEnd - valStart);
        pos = valEnd + 1;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
        {
            ++pos;
        }
        if (pos < json.size() && json[pos] == ',')
        {
            ++pos;
        }
    }
    return true;
}

/**
 * Le um valor numerico (int ou float) e devolve-o como `double`. Reaproveitavel
 * para campos como `StreamingRadiusCm`/`SpeedCmS`.
 */
bool ParseJsonDoubleValue(const std::string& json, const char* key, double& outValue)
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
    bool sawDigit = false;
    bool sawDot = false;
    while (end < json.size())
    {
        const char c = json[end];
        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            sawDigit = true;
            ++end;
        }
        else if (c == '.' && !sawDot)
        {
            sawDot = true;
            ++end;
        }
        else
        {
            break;
        }
    }
    if (!sawDigit)
    {
        return false;
    }
    outValue = std::stod(json.substr(colon, end - colon));
    return true;
}

/**
 * Le um array de waypoints no formato `[[x,y,z], [x,y,z], ...]`. Aceita
 * comentarios/espacos brancos. Sai em silencio com vector vazio se o key nao
 * existir ou o formato for invalido.
 */
bool ParseJsonWaypointsArray(const std::string& json, const char* keyName,
    std::vector<std::array<double, 3>>& out)
{
    out.clear();
    const std::string quotedKey = std::string("\"") + keyName + "\"";
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
    size_t open = json.find('[', colon);
    if (open == std::string::npos)
    {
        return false;
    }
    size_t pos = open + 1;
    auto skipWs = [&json](size_t& p) {
        while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p])))
        {
            ++p;
        }
    };
    while (pos < json.size())
    {
        skipWs(pos);
        if (pos >= json.size())
        {
            return false;
        }
        if (json[pos] == ']')
        {
            return true;
        }
        if (json[pos] == ',')
        {
            ++pos;
            continue;
        }
        if (json[pos] != '[')
        {
            return false;
        }
        ++pos; // entrei no inner array
        std::array<double, 3> tuple{0.0, 0.0, 0.0};
        for (int idx = 0; idx < 3; ++idx)
        {
            skipWs(pos);
            const size_t numStart = pos;
            if (pos < json.size() && (json[pos] == '-' || json[pos] == '+'))
            {
                ++pos;
            }
            bool sawDigit = false;
            bool sawDot = false;
            while (pos < json.size())
            {
                const char c = json[pos];
                if (std::isdigit(static_cast<unsigned char>(c)))
                {
                    sawDigit = true;
                    ++pos;
                }
                else if (c == '.' && !sawDot)
                {
                    sawDot = true;
                    ++pos;
                }
                else
                {
                    break;
                }
            }
            if (!sawDigit)
            {
                return false;
            }
            tuple[idx] = std::stod(json.substr(numStart, pos - numStart));
            skipWs(pos);
            if (idx < 2)
            {
                if (pos >= json.size() || json[pos] != ',')
                {
                    return false;
                }
                ++pos;
            }
        }
        skipWs(pos);
        if (pos >= json.size() || json[pos] != ']')
        {
            return false;
        }
        ++pos; // sai do inner array
        out.push_back(tuple);
    }
    return false;
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
    std::unique_ptr<Zeus::Collision::CollisionWorld> collisionWorld;
    std::unique_ptr<Zeus::Collision::DynamicCollisionWorld> dynamicCollisionWorld;
    std::unique_ptr<Zeus::World::ZeusRegionSystem> regionSystem;
    std::unique_ptr<Zeus::Game::Movement::MovementSystem> movementSystem;
    Zeus::World::DebugPlayerActor* debugPlayer = nullptr; // owned by World
    Zeus::Game::Entities::CharacterActor* debugCharacter = nullptr; // owned by World
    Zeus::World::MapInstance* mainMap = nullptr;          // owned by WorldRuntime
    bool movementEnabled = false;
    Zeus::Net::NetConnectionManager netConnections;
    Zeus::Session::SessionManager sessions;
    Zeus::Session::SessionPacketHandler sessionPacketHandler;
    Zeus::Net::PacketStats netStats{};
    Zeus::Net::NetworkDiagnostics netDiag{};
    Zeus::Net::NetworkSimulator netSim{};
    int targetTps = 30;
    bool initialized = false;
    bool shutDownDone = false;
    bool regionStreamingEnabled = false;
    int debugPanelTickEvery = 30; // emite SetSlot a cada N ticks
    int debugPanelTickCounter = 0;
    int debugOverlayRows = 0;
    std::filesystem::path contentRoot;
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

ZeusResult CoreServerApp::Initialize(const std::filesystem::path& configPath,
    const std::filesystem::path& contentRootIn)
{
    std::error_code rootEc;
    impl_->contentRoot = std::filesystem::weakly_canonical(contentRootIn, rootEc);
    if (rootEc)
    {
        impl_->contentRoot = contentRootIn.lexically_normal();
    }

    ZeusResult readErr = ZeusResult::Success();
    const std::string json = ReadEntireFile(configPath, readErr);
    if (!readErr.Ok())
    {
        return readErr;
    }

    ZeusLog::Info("App",
        std::string("ContentRoot=")
            .append(impl_->contentRoot.string())
            .append(" config=")
            .append(configPath.string()));

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
    const std::filesystem::path sessionLogDir =
        ResolveUnderContentRoot(impl_->contentRoot, std::filesystem::path(sessionLogDirStr));
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

    std::unordered_map<std::string, std::string> worldClientMapPathByName;
    (void)ParseJsonFlatStringObject(json, "WorldClientMapPathByName", worldClientMapPathByName);

    std::string clientMapPath;
    {
        const auto itMapPath = worldClientMapPathByName.find(defaultMap);
        if (itMapPath != worldClientMapPathByName.end())
        {
            clientMapPath = itMapPath->second;
        }
    }

    Zeus::World::MapInstance* mainMap = impl_->worldRuntime->CreateMapInstance(defaultMap);
    if (mainMap != nullptr)
    {
        if (!clientMapPath.empty())
        {
            mainMap->SetClientMapPath(clientMapPath);
        }
        mainMap->BeginPlay();
    }
    impl_->mainMap = mainMap;

    impl_->sessionPacketHandler.SetTravelInfo(defaultMap, clientMapPath);
    ZeusLog::Info("World",
        std::string("World map resolved name=").append(defaultMap).append(" clientPath=").append(clientMapPath.empty() ? "<empty>" : clientMapPath));

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

    bool enableCollisionWorld = false;
    bool runCollisionSmokeTest = false;
    std::string collisionFile;
    std::string dynamicCollisionFile;
    std::string terrainCollisionFile;
    std::unordered_map<std::string, std::string> collisionFileByMap;
    std::unordered_map<std::string, std::string> dynamicFileByMap;
    std::unordered_map<std::string, std::string> terrainFileByMap;
    std::string collisionFileLegacy;
    {
        const std::string collisionSection = "\"Collision\"";
        const size_t cs = json.find(collisionSection);
        if (cs != std::string::npos)
        {
            const std::string scoped = json.substr(cs);
            (void)ParseJsonBoolValue(scoped, "EnableCollisionWorld", enableCollisionWorld);
            (void)ParseJsonBoolValue(scoped, "RunCollisionSmokeTest", runCollisionSmokeTest);
            (void)ParseJsonStringValue(scoped, "CollisionFile", collisionFileLegacy);
            (void)ParseJsonFlatStringObject(scoped, "CollisionFileByMap", collisionFileByMap);
            (void)ParseJsonFlatStringObject(scoped, "DynamicCollisionFileByMap", dynamicFileByMap);
            (void)ParseJsonFlatStringObject(scoped, "TerrainCollisionFileByMap", terrainFileByMap);
        }
    }

    auto resolveByMap = [&defaultMap](const std::unordered_map<std::string, std::string>& m,
        const std::string& fallback) -> std::string {
        const auto it = m.find(defaultMap);
        if (it != m.end())
        {
            return it->second;
        }
        return fallback;
    };

    collisionFile = resolveByMap(collisionFileByMap,
        !collisionFileLegacy.empty() ? collisionFileLegacy
                                     : std::string("Data/Maps/").append(defaultMap).append("/static_collision.zsm"));
    dynamicCollisionFile = resolveByMap(dynamicFileByMap,
        std::string("Data/Maps/").append(defaultMap).append("/dynamic_collision.zsm"));
    terrainCollisionFile = resolveByMap(terrainFileByMap,
        std::string("Data/Maps/").append(defaultMap).append("/terrain_collision.zsm"));

    // RegionStreaming + DebugPlayer config (pre-collision para decidir lazy/build all).
    bool regionStreamingEnabled = false;
    Zeus::World::RegionStreamingSettings regionSettings;
    {
        const size_t rs = json.find("\"RegionStreaming\"");
        if (rs != std::string::npos)
        {
            const std::string scoped = json.substr(rs);
            (void)ParseJsonBoolValue(scoped, "Enabled", regionStreamingEnabled);
            (void)ParseJsonDoubleValue(scoped, "StreamingRadiusCm", regionSettings.StreamingRadiusCm);
            (void)ParseJsonDoubleValue(scoped, "UnloadHysteresisCm", regionSettings.UnloadHysteresisCm);
            int v = regionSettings.MaxLoadsPerTick;
            if (ParseJsonIntValue(scoped, "MaxLoadsPerTick", v)) regionSettings.MaxLoadsPerTick = v;
            v = regionSettings.MaxUnloadsPerTick;
            if (ParseJsonIntValue(scoped, "MaxUnloadsPerTick", v)) regionSettings.MaxUnloadsPerTick = v;
        }
    }
    impl_->regionStreamingEnabled = regionStreamingEnabled;

    bool debugPlayerEnabled = false;
    double debugPlayerSpeedCmS = 600.0;
    std::vector<std::array<double, 3>> debugWaypoints;
    {
        const size_t dp = json.find("\"DebugPlayer\"");
        if (dp != std::string::npos)
        {
            const std::string scoped = json.substr(dp);
            (void)ParseJsonBoolValue(scoped, "Enabled", debugPlayerEnabled);
            (void)ParseJsonDoubleValue(scoped, "SpeedCmS", debugPlayerSpeedCmS);
            (void)ParseJsonWaypointsArray(scoped, "Waypoints", debugWaypoints);
        }
    }

    if (enableCollisionWorld)
    {
        ZeusLog::Info("Collision",
            std::string("Collision path resolved map=").append(defaultMap).append(" zsm=").append(collisionFile));
        impl_->collisionWorld = std::make_unique<Zeus::Collision::CollisionWorld>();

        const std::filesystem::path staticPath =
            ResolveUnderContentRoot(impl_->contentRoot, std::filesystem::path(collisionFile));
        std::error_code fsErr;
        bool assetReady = false;
        if (std::filesystem::exists(staticPath, fsErr))
        {
            assetReady = impl_->collisionWorld->LoadFromZsm(staticPath);
        }
        else
        {
            ZeusLog::Info("Collision",
                std::string("Static .zsm not found: ").append(staticPath.string()));
        }

        const std::filesystem::path terrainPath =
            ResolveUnderContentRoot(impl_->contentRoot, std::filesystem::path(terrainCollisionFile));
        if (std::filesystem::exists(terrainPath, fsErr))
        {
            const bool ok = impl_->collisionWorld->LoadFromTerrainZsm(terrainPath);
            if (ok)
            {
                assetReady = true;
            }
        }
        else
        {
            ZeusLog::Info("Collision",
                std::string("Terrain .zsm not found: ").append(terrainPath.string()));
        }

        const std::filesystem::path dynamicPath =
            ResolveUnderContentRoot(impl_->contentRoot, std::filesystem::path(dynamicCollisionFile));
        if (std::filesystem::exists(dynamicPath, fsErr))
        {
            impl_->dynamicCollisionWorld = std::make_unique<Zeus::Collision::DynamicCollisionWorld>();
            (void)impl_->dynamicCollisionWorld->LoadFromZsm(dynamicPath);
        }
        else
        {
            ZeusLog::Info("Collision",
                std::string("Dynamic .zsm not found: ").append(dynamicPath.string()));
        }

        if (runCollisionSmokeTest && !assetReady)
        {
            assetReady = impl_->collisionWorld->LoadFromAsset(
                Zeus::Collision::CollisionTestScene::BuildProgrammaticAsset());
        }

        if (assetReady)
        {
            if (regionStreamingEnabled)
            {
                (void)impl_->collisionWorld->InitPhysicsLazy();
                ZeusLog::Info("Collision", "[RegionStreaming] Enabled — using lazy region loading.");
            }
            else
            {
                (void)impl_->collisionWorld->BuildPhysicsWorld();
            }
        }

        if (runCollisionSmokeTest && impl_->collisionWorld->IsPhysicsBuilt())
        {
            const auto report = Zeus::Collision::CollisionTestScene::RunAll(*impl_->collisionWorld);
            const std::string summary = std::string("[CollisionTest] passed=")
                .append(std::to_string(report.PassedCount))
                .append(" failed=")
                .append(std::to_string(report.FailedCount));
            if (report.FailedCount == 0)
            {
                ZeusLog::Info("CollisionTest", summary);
            }
            else
            {
                ZeusLog::Warning("CollisionTest", summary);
            }

            // TC-06..TC-09 (streaming + dynamic + terrain)
            if (!impl_->dynamicCollisionWorld)
            {
                impl_->dynamicCollisionWorld = std::make_unique<Zeus::Collision::DynamicCollisionWorld>();
                impl_->dynamicCollisionWorld->LoadFromAsset(
                    Zeus::Collision::CollisionTestScene::BuildProgrammaticDynamicAsset());
            }
            const auto streamReport = Zeus::Collision::CollisionTestScene::RunStreamingScenarios(
                *impl_->collisionWorld, impl_->dynamicCollisionWorld.get());
            (void)streamReport;

            // RegionSystemTests usam um World/CollisionWorld separados (nao tocam no estado do servidor).
            (void)Zeus::World::RegionSystemTests::RunAll();
        }
    }

    // --- Movement (BL-003 / Parte 5) ---------------------------------------
    bool movementEnabled = false;
    bool movementRunSmokeTests = false;
    bool movementSpawnDebugCharacter = false;
    Zeus::Game::Movement::MovementParameters movementParams;
    {
        const size_t mv = json.find("\"Movement\"");
        if (mv != std::string::npos)
        {
            const std::string scoped = json.substr(mv);
            (void)ParseJsonBoolValue(scoped, "Enabled", movementEnabled);
            (void)ParseJsonBoolValue(scoped, "RunSmokeTests", movementRunSmokeTests);
            (void)ParseJsonBoolValue(scoped, "SpawnDebugCharacter", movementSpawnDebugCharacter);
            (void)ParseJsonDoubleValue(scoped, "GravityZ", movementParams.GravityZ);
            (void)ParseJsonDoubleValue(scoped, "MaxSlopeAngleDeg", movementParams.MaxSlopeAngleDeg);
            (void)ParseJsonDoubleValue(scoped, "StepHeightCm", movementParams.StepHeightCm);
            (void)ParseJsonDoubleValue(scoped, "DefaultSpeedCmS", movementParams.DefaultSpeedCmS);
            (void)ParseJsonDoubleValue(scoped, "JumpVelocityCmS", movementParams.JumpVelocityCmS);
            const size_t cap = scoped.find("\"Capsule\"");
            if (cap != std::string::npos)
            {
                const std::string capScoped = scoped.substr(cap);
                (void)ParseJsonDoubleValue(capScoped, "RadiusCm", movementParams.CapsuleRadiusCm);
                (void)ParseJsonDoubleValue(capScoped, "HalfHeightCm", movementParams.CapsuleHalfHeightCm);
            }
        }
    }
    impl_->movementEnabled = movementEnabled;

    if (movementEnabled)
    {
        impl_->movementSystem = std::make_unique<Zeus::Game::Movement::MovementSystem>();
        impl_->movementSystem->Configure(movementParams);
        if (impl_->collisionWorld)
        {
            impl_->movementSystem->SetCollisionWorld(impl_->collisionWorld.get());
        }
        ZeusLog::Info("Movement",
            std::string("MovementSystem configured gravity=")
                .append(std::to_string(movementParams.GravityZ))
                .append(" maxSlope=")
                .append(std::to_string(movementParams.MaxSlopeAngleDeg))
                .append(" stepHeight=")
                .append(std::to_string(movementParams.StepHeightCm))
                .append(" speed=")
                .append(std::to_string(movementParams.DefaultSpeedCmS)));

        ZeusLog::Info("Movement",
            std::string("RunSmokeTests=").append(movementRunSmokeTests ? "true" : "false"));
        if (movementRunSmokeTests)
        {
            // MovementTests cria o seu proprio CollisionWorld interno (asset
            // programatico). Passamos o do servidor apenas como referencia
            // (signature exigida); o teste e' independente de Jolt global.
            Zeus::Collision::CollisionWorld dummy;
            Zeus::Collision::CollisionWorld& ref = impl_->collisionWorld
                ? *impl_->collisionWorld : dummy;
            (void)Zeus::Game::Movement::MovementTests::RunAll(ref);
        }

        if (movementSpawnDebugCharacter && mainMap != nullptr && impl_->collisionWorld)
        {
            Zeus::World::SpawnParameters params;
            params.Name = "DebugCharacter";
            params.Transform.Location = Zeus::Math::Vector3(0.0, 0.0, 200.0);
            params.bAllowTick = true;
            params.bStartActive = true;
            auto* ch = mainMap->GetWorld().SpawnActorTyped<Zeus::Game::Entities::CharacterActor>(params, true);
            if (ch != nullptr)
            {
                ch->SetCapsuleSize(movementParams.CapsuleRadiusCm, movementParams.CapsuleHalfHeightCm);
                if (auto* mc = ch->GetMovementComponent())
                {
                    mc->SetCollisionWorld(impl_->collisionWorld.get());
                    mc->SetParameters(movementParams);
                    mc->MoveByAxis(1.0, 0.0); // andar em frente para validar runtime
                }
                impl_->debugCharacter = ch;
                ZeusLog::Info("Movement", "Spawned DebugCharacter at (0,0,200) moving forward.");
            }
        }
    }
    else
    {
        ZeusLog::Info("Collision", "EnableCollisionWorld=false; CollisionWorld not constructed.");
    }

    if (regionStreamingEnabled && impl_->collisionWorld)
    {
        impl_->regionSystem = std::make_unique<Zeus::World::ZeusRegionSystem>();
        regionSettings.RegionSizeCm = impl_->collisionWorld->GetAsset().RegionSizeCm > 0.0
            ? impl_->collisionWorld->GetAsset().RegionSizeCm
            : 5000.0;
        impl_->regionSystem->Configure(regionSettings);
        ZeusLog::Info("RegionSystem",
            std::string("Configured RegionSizeCm=")
                .append(std::to_string(regionSettings.RegionSizeCm))
                .append(" StreamingRadiusCm=")
                .append(std::to_string(regionSettings.StreamingRadiusCm)));
    }

    if (debugPlayerEnabled && mainMap != nullptr)
    {
        Zeus::World::SpawnParameters params;
        params.Name = "DebugPlayer";
        if (!debugWaypoints.empty())
        {
            const auto& w0 = debugWaypoints.front();
            params.Transform.Location = Zeus::Math::Vector3(w0[0], w0[1], w0[2]);
        }
        params.bAllowTick = true;
        params.bStartActive = true;
        Zeus::World::DebugPlayerActor* dp = mainMap->GetWorld().SpawnActorTyped<Zeus::World::DebugPlayerActor>(params, true);
        if (dp != nullptr)
        {
            std::vector<Zeus::Math::Vector3> wpts;
            wpts.reserve(debugWaypoints.size());
            for (const auto& t : debugWaypoints)
            {
                wpts.emplace_back(t[0], t[1], t[2]);
            }
            dp->SetWaypoints(std::move(wpts), debugPlayerSpeedCmS);
            impl_->debugPlayer = dp;
            ZeusLog::Info("DebugPlayer",
                std::string("Spawned DebugPlayerActor waypoints=")
                    .append(std::to_string(debugWaypoints.size()))
                    .append(" speedCmS=")
                    .append(std::to_string(debugPlayerSpeedCmS)));
        }
    }
    impl_->debugOverlayRows = overlayRows;

    impl_->loop->SetFixedTickCallback([this](const double fixedDeltaSeconds) { RunFixedTick(fixedDeltaSeconds); });

    ZeusLog::Info("App", "CoreServerApp initialized (Part 1 + Part 2 network + Part 3 world runtime + Part 4 collision).");
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
    impl_->movementSystem.reset();
    impl_->debugCharacter = nullptr;
    impl_->regionSystem.reset();
    if (impl_->dynamicCollisionWorld)
    {
        impl_->dynamicCollisionWorld->Shutdown();
        impl_->dynamicCollisionWorld.reset();
    }
    if (impl_->collisionWorld)
    {
        impl_->collisionWorld->Shutdown();
        impl_->collisionWorld.reset();
    }
    impl_->debugPlayer = nullptr;
    impl_->mainMap = nullptr;
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

    // Streaming so corre depois do worldTick (DebugPlayer ja moveu).
    if (impl_->regionSystem && impl_->collisionWorld && impl_->mainMap != nullptr)
    {
        impl_->regionSystem->Tick(impl_->mainMap->GetWorld(),
            *impl_->collisionWorld,
            impl_->dynamicCollisionWorld.get(),
            fixedDeltaSeconds);
    }

    if (impl_->movementSystem && impl_->mainMap != nullptr)
    {
        impl_->movementSystem->RefreshStats(impl_->mainMap->GetWorld());
    }

    // ConsoleLivePanel slots de stats (cada N ticks).
    if (impl_->debugOverlayRows > 1)
    {
        ++impl_->debugPanelTickCounter;
        if (impl_->debugPanelTickCounter >= impl_->debugPanelTickEvery)
        {
            impl_->debugPanelTickCounter = 0;

            std::ostringstream slot2;
            if (impl_->regionSystem)
            {
                const auto& s = impl_->regionSystem->GetStats();
                slot2 << "Region active=" << s.RegionsActive
                      << " bodies=" << s.BodiesActive
                      << " loads=" << s.TotalLoads
                      << " unloads=" << s.TotalUnloads;
            }
            else if (impl_->collisionWorld)
            {
                slot2 << "Collision bodies=" << impl_->collisionWorld->GetStaticBodyCount();
            }
            else
            {
                slot2 << "Region streaming disabled";
            }
            Zeus::Platform::ConsoleLivePanel::SetSlot(1, slot2.str());

            if (impl_->debugOverlayRows > 2)
            {
                std::ostringstream slot3;
                if (impl_->debugPlayer != nullptr)
                {
                    const Zeus::Math::Vector3 p = impl_->debugPlayer->GetLocation();
                    slot3 << "DebugPlayer pos=" << static_cast<long long>(p.X)
                          << "," << static_cast<long long>(p.Y)
                          << "," << static_cast<long long>(p.Z);
                    if (impl_->regionSystem)
                    {
                        slot3 << " region=" << impl_->regionSystem->GetPrimaryPlayerRegionId();
                    }
                }
                else
                {
                    slot3 << "No DebugPlayer";
                }
                Zeus::Platform::ConsoleLivePanel::SetSlot(2, slot3.str());
            }

            if (impl_->debugOverlayRows > 3)
            {
                std::ostringstream slot4;
                if (impl_->debugCharacter != nullptr && impl_->debugCharacter->GetMovementComponent() != nullptr)
                {
                    const Zeus::Math::Vector3 p = impl_->debugCharacter->GetLocation();
                    const auto& v = impl_->debugCharacter->GetMovementComponent()->GetVelocity();
                    slot4 << "Move pos=(" << static_cast<long long>(p.X)
                          << "," << static_cast<long long>(p.Y)
                          << "," << static_cast<long long>(p.Z)
                          << ") vel=(" << static_cast<long long>(v.X)
                          << "," << static_cast<long long>(v.Y)
                          << "," << static_cast<long long>(v.Z)
                          << ") grounded=" << (impl_->debugCharacter->GetMovementComponent()->IsGrounded() ? 1 : 0);
                }
                else if (impl_->movementSystem)
                {
                    const auto& s = impl_->movementSystem->GetStats();
                    slot4 << "Move chars=" << s.CharacterCount
                          << " grounded=" << s.GroundedCount
                          << " sweeps=" << s.SweepsLastTick
                          << " avgVel=" << static_cast<long long>(s.AvgVelocityCmS);
                }
                else
                {
                    slot4 << "Movement disabled";
                }
                Zeus::Platform::ConsoleLivePanel::SetSlot(3, slot4.str());
            }
        }
    }
}
} // namespace Zeus::App
