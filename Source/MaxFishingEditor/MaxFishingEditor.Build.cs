// Copyright MaxFishing Project

using UnrealBuildTool;

public class MaxFishingEditor : ModuleRules
{
	public MaxFishingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"AssetTools",
			"MaxFishing"
		});
	}
}
