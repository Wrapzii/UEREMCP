using UnrealBuildTool;

public class UeremcpValidationProbe : ModuleRules
{
	public UeremcpValidationProbe(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Launch-smoke only. Does NOT depend on FileSandbox / ToolsetRegistry —
		// shipping Rollback.MultiAssetDiscard lives in UeremcpValidation.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});
	}
}
