using UnrealBuildTool;

// Editor-only helpers for building the level's assets. Today: the .rrgroom
// translator, so the sasquatch's generated fur imports as a Groom asset.
public class RedRocksEditor : ModuleRules
{
	public RedRocksEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "MeshDescription", "HairStrandsCore", "HairStrandsEditor"
		});
	}
}
