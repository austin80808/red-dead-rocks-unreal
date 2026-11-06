using UnrealBuildTool;

public class RedRocksEditorTarget : TargetRules
{
	public RedRocksEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RedRocks", "RedRocksEditor" });
	}
}
