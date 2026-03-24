// Copyright MaxFishing Project

using UnrealBuildTool;
using System.Collections.Generic;

public class MaxFishingEditorTarget : TargetRules
{
	public MaxFishingEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		// Match UnrealEditor C++ warning defaults (incl. UndefinedIdentifierWarningLevel = Error)
		// under the normal Shared build environment. Avoid TargetBuildEnvironment.Unique unless
		// Epic support asks for it — Unique can trigger follow-on UBT/VS issues.
		DefaultBuildSettings = BuildSettingsVersion.Latest;

		ExtraModuleNames.AddRange(new string[] { "MaxFishing", "MaxFishingEditor" });
	}
}
