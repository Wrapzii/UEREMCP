// UEREMCP — Blueprint domain module (WS-06).
// Composes Epic BlueprintTools; agent-facing read/submit graph operations (ADR-0002).

using UnrealBuildTool;

public class UeremcpBlueprint : ModuleRules
{
	public UeremcpBlueprint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Json",
			"JsonUtilities",
			"Projects",

			// ADR-0003 envelope — no ToolsetRegistry coupling in Protocol.
			"UeremcpProtocol",

			// Epic tool-declaration layer. ADR-0002.
			// [VERIFIED: $TR/ToolsetRegistry.uplugin — Editor module]
			"ToolsetRegistry",
		});
	}
}
