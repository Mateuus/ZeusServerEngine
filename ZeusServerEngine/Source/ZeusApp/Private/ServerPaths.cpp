#include "ServerPaths.hpp"

#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace Zeus::App
{
namespace
{
std::filesystem::path ExecutableDirectory(const char* argv0)
{
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
    {
        return std::filesystem::path(buf).parent_path();
    }
    (void)argv0;
#else
    std::error_code ec;
    if (argv0 != nullptr && argv0[0] != '\0')
    {
        const std::filesystem::path abs = std::filesystem::absolute(std::filesystem::path(argv0), ec);
        if (!abs.empty())
        {
            return abs.parent_path();
        }
    }
#endif
    return std::filesystem::current_path();
}

bool FileExists(const std::filesystem::path& p, std::error_code& ec)
{
    return std::filesystem::is_regular_file(p, ec);
}

bool IsDataDir(const std::filesystem::path& root, std::error_code& ec)
{
    return std::filesystem::is_directory(root / "Data", ec);
}
} // namespace

std::filesystem::path ResolveContentRoot(const char* argv0)
{
    if (const char* env = std::getenv("ZEUS_SERVER_ROOT"))
    {
        std::filesystem::path p(env);
        if (!p.empty())
        {
            std::error_code ec;
            std::filesystem::path c = std::filesystem::weakly_canonical(p, ec);
            return ec ? p.lexically_normal() : c;
        }
    }

    std::filesystem::path start = ExecutableDirectory(argv0);
    std::error_code ec;

    std::filesystem::path fallback;
    for (int i = 0; i < 10; ++i)
    {
        const std::filesystem::path cfg = start / "Config" / "server.json";
        if (FileExists(cfg, ec))
        {
            if (IsDataDir(start, ec))
            {
                std::filesystem::path c = std::filesystem::weakly_canonical(start, ec);
                return ec ? start.lexically_normal() : c;
            }
            if (fallback.empty())
            {
                fallback = start;
            }
        }

        const std::filesystem::path parent = start.parent_path();
        if (parent == start)
        {
            break;
        }
        start = parent;
    }

    if (!fallback.empty())
    {
        std::filesystem::path c = std::filesystem::weakly_canonical(fallback, ec);
        return ec ? fallback.lexically_normal() : c;
    }

    return std::filesystem::current_path();
}

std::filesystem::path ResolveUnderContentRoot(const std::filesystem::path& contentRoot,
    const std::filesystem::path& p)
{
    if (p.empty())
    {
        return p;
    }
    if (p.is_absolute())
    {
        return p;
    }
    return (contentRoot / p).lexically_normal();
}
} // namespace Zeus::App
