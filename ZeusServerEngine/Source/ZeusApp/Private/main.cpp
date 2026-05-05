#include "CoreServerApp.hpp"

#include "ZeusLog.hpp"

#include <filesystem>
#include <string_view>

int main(int argc, char** argv)
{
    std::filesystem::path configPath = "Config/server.json";
    if (argc >= 2)
    {
        configPath = argv[1];
    }

    Zeus::App::CoreServerApp app;
    const Zeus::ZeusResult initResult = app.Initialize(configPath);
    if (!initResult.Ok())
    {
        Zeus::ZeusLog::Error("App", std::string_view(initResult.GetErrorText()));
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
