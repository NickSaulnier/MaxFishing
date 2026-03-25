// Copyright MaxFishing Project

using System.IO;
using UnrealBuildTool;

public class MaxFishing : ModuleRules
{
	public MaxFishing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(new string[]
		{
			Path.Combine(ModuleDirectory, "Player"),
			Path.Combine(ModuleDirectory, "Fishing"),
			Path.Combine(ModuleDirectory, "Fish"),
			Path.Combine(ModuleDirectory, "Audio")
		});

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
			"AudioExtensions",
			"Landscape",
			"RHI"
		});
	}
}
