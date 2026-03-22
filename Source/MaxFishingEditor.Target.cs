// Copyright MaxFishing Project

using UnrealBuildTool;
using System.Collections.Generic;

public class MaxFishingEditorTarget : TargetRules
{
	public MaxFishingEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		ExtraModuleNames.AddRange(new string[] { "MaxFishing" });
	}
}
