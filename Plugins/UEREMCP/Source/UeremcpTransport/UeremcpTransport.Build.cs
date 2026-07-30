// UEREMCP — transport capability probe and job-model constraints (WS-04).
// Does NOT implement MCP transport; Epic's ModelContextProtocol owns that (ADR-0002).

using UnrealBuildTool;

public class UeremcpTransport : ModuleRules
{
	public UeremcpTransport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"UeremcpProtocol",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"Projects",
			"UeremcpCore",
			"ToolsetRegistry",

			// Public Epic MCP surface only — introspection, not reimplementation.
			"ModelContextProtocol",
			"ModelContextProtocolEngine",
		});
	}
}
