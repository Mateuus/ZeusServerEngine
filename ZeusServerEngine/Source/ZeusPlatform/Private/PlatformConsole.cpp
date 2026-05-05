#include "PlatformConsole.hpp"

#include "ConsoleIo.hpp"

#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace Zeus::Platform
{
void SetConsoleTitleUtf8(std::string_view titleUtf8)
{
    std::lock_guard lock(Zeus::Platform::Detail::ConsoleIoMutex());
    std::string title(titleUtf8);
    constexpr std::size_t kMaxTitleChars = 240;
    if (title.size() > kMaxTitleChars)
    {
        title.resize(kMaxTitleChars);
    }

#if defined(_WIN32)
    const int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.c_str(), static_cast<int>(title.size()),
        nullptr, 0);
    if (wideLen <= 0)
    {
        return;
    }
    std::wstring wide(static_cast<std::size_t>(wideLen), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.c_str(), static_cast<int>(title.size()), wide.data(),
            wideLen) <= 0)
    {
        return;
    }
    SetConsoleTitleW(wide.c_str());
#else
    // OSC 0 — muitos terminais Unix atualizam o título do separador com isto.
    std::string seq = "\x1b]0;";
    seq += title;
    seq += '\007';
    // Mesmo fluxo que ZeusLog (stderr) para o terminal tratar título e logs de forma coerente.
    (void)::write(STDERR_FILENO, seq.data(), seq.size());
#endif
}
} // namespace Zeus::Platform
