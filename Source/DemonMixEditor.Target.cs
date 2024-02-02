

using UnrealBuildTool;
using System.Collections.Generic;

public class DemonMixEditorTarget : TargetRules
{
    public DemonMixEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V4;

        ExtraModuleNames.AddRange(new string[] { "DemonMix" });
    }
}
