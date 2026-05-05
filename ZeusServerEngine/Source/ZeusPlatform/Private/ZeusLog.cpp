#include "ZeusLog.hpp"

#include "ConsoleIo.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace Zeus
{
namespace
{
LogLevel g_minLevel = LogLevel::Info;
std::ofstream g_sessionLog;
std::filesystem::path g_sessionPath;

void CloseSessionLogUnlocked()
{
    if (g_sessionLog.is_open())
    {
        g_sessionLog.flush();
        g_sessionLog.close();
    }
    g_sessionPath.clear();
}

[[nodiscard]] std::string MakeSessionLogFileName()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
    struct tm tmStorage {};
    if (localtime_s(&tmStorage, &t) != 0)
    {
        return "ZeusServer_unknown.log";
    }
    const struct tm* tmPtr = &tmStorage;
#else
    struct tm tmStorage {};
    if (localtime_r(&t, &tmStorage) == nullptr)
    {
        return "ZeusServer_unknown.log";
    }
    const struct tm* tmPtr = &tmStorage;
#endif
    char buf[96] = {};
    std::strftime(buf, sizeof(buf), "ZeusServer_%Y%m%d_%H%M%S.log", tmPtr);
    return std::string(buf);
}
} // namespace

void ZeusLog::SetMinimumLevel(LogLevel level)
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    g_minLevel = level;
}

LogLevel ZeusLog::GetMinimumLevel()
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    return g_minLevel;
}

const char* ZeusLog::LevelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace:
        return "Trace";
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warn";
    case LogLevel::Error:
        return "Error";
    default:
        return "?";
    }
}

ZeusResult ZeusLog::OpenSessionLog(const std::filesystem::path& logDirectory)
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    CloseSessionLogUnlocked();

    std::error_code ec;
    std::filesystem::create_directories(logDirectory, ec);
    if (ec)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "OpenSessionLog: create_directories failed: " + ec.message());
    }

    const std::filesystem::path filePath = logDirectory / MakeSessionLogFileName();
    g_sessionLog.open(filePath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!g_sessionLog)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "OpenSessionLog: failed to open file for write.");
    }
    g_sessionPath = filePath;
    return ZeusResult::Success();
}

void ZeusLog::CloseSessionLog()
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    CloseSessionLogUnlocked();
}

bool ZeusLog::IsSessionLogOpen()
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    return g_sessionLog.is_open();
}

std::filesystem::path ZeusLog::GetSessionLogPath()
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    return g_sessionPath;
}

void ZeusLog::Write(LogLevel level, std::string_view category, std::string_view message)
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    if (level < g_minLevel)
    {
        return;
    }
    char timeBuf[32] = {};
#if defined(_WIN32)
    struct tm tmStorage {};
    if (localtime_s(&tmStorage, &t) == 0)
    {
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmStorage);
    }
#else
    if (std::tm* tmPtr = std::localtime(&t))
    {
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", tmPtr);
    }
#endif

    const int n = std::fprintf(stderr, "%s [%s] [%.*s] %.*s\n", timeBuf, LevelName(level), static_cast<int>(category.size()),
        category.data(), static_cast<int>(message.size()), message.data());
    (void)n;
    std::fflush(stderr);

    if (g_sessionLog.is_open())
    {
        g_sessionLog << timeBuf << " [" << LevelName(level) << "] [";
        g_sessionLog.write(category.data(), static_cast<std::streamsize>(category.size()));
        g_sessionLog << "] ";
        g_sessionLog.write(message.data(), static_cast<std::streamsize>(message.size()));
        g_sessionLog << '\n';
        g_sessionLog.flush();
    }
}

void ZeusLog::Trace(std::string_view category, std::string_view message)
{
    Write(LogLevel::Trace, category, message);
}

void ZeusLog::Debug(std::string_view category, std::string_view message)
{
    Write(LogLevel::Debug, category, message);
}

void ZeusLog::Info(std::string_view category, std::string_view message)
{
    Write(LogLevel::Info, category, message);
}

void ZeusLog::Warning(std::string_view category, std::string_view message)
{
    Write(LogLevel::Warning, category, message);
}

void ZeusLog::Error(std::string_view category, std::string_view message)
{
    Write(LogLevel::Error, category, message);
}
} // namespace Zeus
