using UnrealBuildTool;

public class UeremcpValidationProbe : ModuleRules
{
	public UeremcpValidationProbe(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"EditorScriptingUtilities",
			"ToolsetRegistry",
			"FileSandboxCore",
		});
	}
}
