#include "PlatformConsoleLive.hpp"

#include "ConsoleIo.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace Zeus::Platform
{
namespace
{
bool g_panelActive = false;
std::uint32_t g_reservedRows = 0;
int g_termCols = 80;
int g_termRows = 25;
int g_firstOverlayRow1Based = 1;
#if defined(_WIN32)
DWORD g_savedStderrMode = 0;
bool g_hasSavedStderrMode = false;
#endif

[[nodiscard]] bool StdErrIsTTY()
{
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(STDERR_FILENO) != 0;
#endif
}

[[nodiscard]] bool QueryTerminalSize(int& outCols, int& outRows)
{
#if defined(_WIN32)
    const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
    CONSOLE_SCREEN_BUFFER_INFO info {};
    if (h == INVALID_HANDLE_VALUE || h == nullptr || !GetConsoleScreenBufferInfo(h, &info))
    {
        return false;
    }
    outCols = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
    outRows = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
    if (outCols < 8)
    {
        outCols = 80;
    }
    if (outRows < 2)
    {
        outRows = 25;
    }
    return true;
#else
    winsize ws {};
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_row <= 0 || ws.ws_col <= 0)
    {
        return false;
    }
    outCols = ws.ws_col;
    outRows = ws.ws_row;
    return true;
#endif
}

void WriteRaw(std::string_view data)
{
    if (!data.empty())
    {
        (void)std::fwrite(data.data(), 1, data.size(), stderr);
    }
}

void ShutdownUnlocked();

#if defined(_WIN32)
void EnableVtOnStdErr()
{
    const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
    if (h == INVALID_HANDLE_VALUE || h == nullptr)
    {
        return;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode))
    {
        return;
    }
    g_savedStderrMode = mode;
    g_hasSavedStderrMode = true;
    (void)SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void RestoreStdErrMode()
{
    if (!g_hasSavedStderrMode)
    {
        return;
    }
    const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
    if (h != INVALID_HANDLE_VALUE && h != nullptr)
    {
        (void)SetConsoleMode(h, g_savedStderrMode);
    }
    g_hasSavedStderrMode = false;
}
#endif

void ShutdownUnlocked()
{
    if (!g_panelActive)
    {
        return;
    }
    WriteRaw("\x1b[r");
    WriteRaw("\x1b[0m");
#if defined(_WIN32)
    RestoreStdErrMode();
#endif
    (void)std::fflush(stderr);
    g_panelActive = false;
    g_reservedRows = 0;
}

void WriteLineAtRow1Based(int row1Based, std::string_view text, bool clearWholeLine)
{
    char move[48] = {};
    (void)std::snprintf(move, sizeof(move), "\x1b\x37\x1b[%d;1H", row1Based);
    WriteRaw(move);
    if (clearWholeLine)
    {
        WriteRaw("\x1b[2K");
    }
    std::string line(text);
    const int maxBytes = std::max(4, g_termCols - 1);
    if (static_cast<int>(line.size()) > maxBytes)
    {
        line.resize(static_cast<std::size_t>(std::max(0, maxBytes - 3)));
        line += "...";
    }
    WriteRaw(line);
    WriteRaw("\x1b\x38");
    (void)std::fflush(stderr);
}
} // namespace

ZeusResult ConsoleLivePanel::Init(const std::uint32_t reservedRows)
{
    std::lock_guard lock(Detail::ConsoleIoMutex());
    ShutdownUnlocked();

    if (reservedRows == 0)
    {
        return ZeusResult::Success();
    }
    if (!StdErrIsTTY())
    {
        return ZeusResult::Success();
    }

    if (!QueryTerminalSize(g_termCols, g_termRows))
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "ConsoleLivePanel: could not query terminal size.");
    }

    if (reservedRows >= static_cast<std::uint32_t>(g_termRows))
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument,
            "ConsoleLivePanel: reservedRows too large for terminal height.");
    }

#if defined(_WIN32)
    EnableVtOnStdErr();
#endif

    g_firstOverlayRow1Based = g_termRows - static_cast<int>(reservedRows) + 1;
    int scrollEnd = g_firstOverlayRow1Based - 1;
    if (scrollEnd < 1)
    {
        scrollEnd = 1;
    }

    char margin[64] = {};
    (void)std::snprintf(margin, sizeof(margin), "\x1b[1;%dr", scrollEnd);
    WriteRaw(margin);
    WriteRaw("\x1b[H");
    (void)std::fflush(stderr);

    g_reservedRows = reservedRows;
    g_panelActive = true;
    return ZeusResult::Success();
}

void ConsoleLivePanel::Shutdown()
{
    std::lock_guard lock(Detail::ConsoleIoMutex());
    ShutdownUnlocked();
}

void ConsoleLivePanel::SetSlot(const std::uint32_t index, const std::string_view lineUtf8)
{
    std::lock_guard lock(Detail::ConsoleIoMutex());
    if (!g_panelActive || index >= g_reservedRows)
    {
        return;
    }
    const int row = g_firstOverlayRow1Based + static_cast<int>(index);
    WriteLineAtRow1Based(row, lineUtf8, true);
}

void ConsoleLivePanel::ClearSlot(const std::uint32_t index)
{
    std::lock_guard lock(Detail::ConsoleIoMutex());
    if (!g_panelActive || index >= g_reservedRows)
    {
        return;
    }
    const int row = g_firstOverlayRow1Based + static_cast<int>(index);
    WriteLineAtRow1Based(row, "", true);
}

void ConsoleLivePanel::SetHeader(const std::string_view lineUtf8)
{
    std::lock_guard lock(Detail::ConsoleIoMutex());
    if (!g_panelActive)
    {
        return;
    }
    const int headerRow = g_firstOverlayRow1Based - 1;
    if (headerRow < 1)
    {
        return;
    }
    WriteLineAtRow1Based(headerRow, lineUtf8, true);
}
} // namespace Zeus::Platform
