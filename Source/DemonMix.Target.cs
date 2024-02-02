

using UnrealBuildTool;
using System.Collections.Generic;

public class DemonMixTarget : TargetRules
{
    public DemonMixTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V4;

        ExtraModuleNames.AddRange(new string[] { "DemonMix" });
    }
}
