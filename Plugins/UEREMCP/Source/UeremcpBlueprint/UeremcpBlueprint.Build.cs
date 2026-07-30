// UEREMCP — Blueprint domain module (WS-06).
// Native C++ submit plus optional Epic BlueprintTools read enrichment (ADR-0002).

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
			"BlueprintGraph",
			"Kismet",
			"BlueprintEditorLibrary",
			"AssetRegistry",

			// ADR-0003 envelope — no ToolsetRegistry coupling in Protocol.
			"UeremcpProtocol",

			// FUeremcpMutatingDispatch (orch UeremcpCore) + permission/path deps.
			"UeremcpCore",
			"UeremcpSecurity",

			// Epic tool-declaration layer. ADR-0002.
			// [VERIFIED: $TR/ToolsetRegistry.uplugin — Editor module]
			"ToolsetRegistry",
		});

		PublicDefinitions.Add("UEREMCP_BLUEPRINT_MUTATING_DISPATCH=1");
	}
}
