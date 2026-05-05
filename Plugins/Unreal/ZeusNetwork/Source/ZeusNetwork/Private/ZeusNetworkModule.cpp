#include "ZeusNetwork.h"
#include "Modules/ModuleManager.h"
#include "ZeusNetworkLog.h"

DEFINE_LOG_CATEGORY(LogZeusNetwork);

class FZeusNetworkModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FZeusNetworkModule, ZeusNetwork)
