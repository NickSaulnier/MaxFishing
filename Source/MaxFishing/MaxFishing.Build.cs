// Copyright MaxFishing Project

using UnrealBuildTool;

public class MaxFishing : ModuleRules
{
	public MaxFishing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			"CableComponent",
			"Water",
			"Niagara",
			"AudioMixer",
			"MetasoundEngine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UMG",
			"AudioExtensions"
		});
	}
}
