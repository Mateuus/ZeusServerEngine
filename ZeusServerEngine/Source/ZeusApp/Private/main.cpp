#include "CoreServerApp.hpp"
#include "ServerPaths.hpp"

#include "ZeusLog.hpp"

#include <filesystem>
#include <string_view>

int main(int argc, char** argv)
{
    const char* argv0 = (argc > 0 && argv[0] != nullptr) ? argv[0] : "";
    std::filesystem::path contentRoot = Zeus::App::ResolveContentRoot(argv0);
    std::filesystem::path configPath = contentRoot / "Config" / "server.json";

    if (argc >= 2)
    {
        configPath = std::filesystem::path(argv[1]);
        if (configPath.is_relative())
        {
            std::error_code ec;
            configPath = std::filesystem::absolute(configPath, ec);
        }
        if (configPath.filename() == "server.json")
        {
            const std::filesystem::path cfgDir = configPath.parent_path();
            if (cfgDir.filename() == "Config")
            {
                contentRoot = cfgDir.parent_path();
            }
        }
    }

    {
        std::error_code ec;
        contentRoot = std::filesystem::weakly_canonical(contentRoot, ec);
        if (ec)
        {
            contentRoot = contentRoot.lexically_normal();
        }
    }

    Zeus::App::CoreServerApp app;
    const Zeus::ZeusResult initResult = app.Initialize(configPath, contentRoot);
    if (!initResult.Ok())
    {
        Zeus::ZeusLog::Error("App", std::string_view(initResult.GetErrorText()));
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
