// UEREMCP — Material domain module (WS-08).
//
// Wave 2: MaterialEditingLibrary substrate (equivalent to Epic MaterialTools batching).

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
			// GEditor, Editor.h, EditorAssetSubsystem [VERIFIED: UnrealEd module].
			// Do NOT add module "Editor" — UBT 5.8 has no such module
			// [VERIFIED-RUNTIME: UBT 2026-07-30, "Could not find definition for module 'Editor'"].
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"Projects",

			"UeremcpCore",
			"UeremcpSecurity",
			"ToolsetRegistry",
			"UeremcpProtocol",

			// Epic material editor surface [VERIFIED: MaterialEditingLibrary.h].
			"MaterialEditor",
		});
	}
}
