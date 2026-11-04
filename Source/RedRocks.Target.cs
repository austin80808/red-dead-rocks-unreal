using UnrealBuildTool;

public class RedRocksTarget : TargetRules
{
	public RedRocksTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RedRocks");
	}
}
