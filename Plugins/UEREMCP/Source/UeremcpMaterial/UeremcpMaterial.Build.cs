// UEREMCP — Material domain module (WS-08).
//
// Wave 2 scaffold: thin toolset over Epic MaterialTools + MaterialEditingLibrary
// batching. Primitives stay internal (ADR-0002 rule 5).

using UnrealBuildTool;

public class UeremcpMaterial : ModuleRules
{
	public UeremcpMaterial(ReadOnlyTargetRules Target) : base(Target)
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

			"UeremcpCore",
			// Epic's tool-declaration layer (UToolsetDefinition / AICallable). ADR-0002.
			"ToolsetRegistry",

			"UeremcpProtocol",

			// Epic material editor surface — used when create_vfx_material is implemented.
			// MaterialTools (EditorToolset) must be enabled in the target project.
			"MaterialEditor",
		});
	}
}
