using UnrealBuildTool;

public class ZeusNetwork : ModuleRules
{
	public ZeusNetwork(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking",
			"Projects",
			"DeveloperSettings",
			"ApplicationCore"
		});
	}
}
