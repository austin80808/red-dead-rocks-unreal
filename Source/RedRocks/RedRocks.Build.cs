using UnrealBuildTool;

public class RedRocks : ModuleRules
{
	public RedRocks(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "EnhancedInput", "UMG", "Slate", "SlateCore"
		});
	}
}
