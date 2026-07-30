// UEREMCP — security & reliability module (WS-12, ADR-0010).
//
// Application-layer permission tiers, path policy, mutator queue, and audit log.
// Depends on UeremcpProtocol for envelope shapes; does NOT depend on
// ModelContextProtocol (no transport fork — ADR-0002).

using UnrealBuildTool;

public class UeremcpSecurity : ModuleRules
{
	public UeremcpSecurity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"Json",
			"JsonUtilities",

			// Envelope action/mode/options shapes — no ToolsetRegistry here.
			"UeremcpProtocol",
		});
	}
}
