using UnrealBuildTool;

public class ZeusMapTools : ModuleRules
{
	public ZeusMapTools(ReadOnlyTargetRules Target) : base(Target)
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
			"ApplicationCore",
			"UnrealEd",
			"EditorFramework",
			"EditorSubsystem",
			"LevelEditor",
			"Projects",
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"InputCore",
			"PhysicsCore",
			"Landscape",
			"RenderCore",
			"RHI"
		});
	}
}
