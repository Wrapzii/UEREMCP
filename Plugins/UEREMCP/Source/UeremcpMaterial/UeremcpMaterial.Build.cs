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
			"UnrealEd",
			"AssetTools",
			"Json",
			"JsonUtilities",
			"Projects",

			"UeremcpCore",
			"ToolsetRegistry",
			"UeremcpProtocol",

			// Epic material editor surface [VERIFIED: MaterialEditingLibrary.h].
			"MaterialEditor",
		});
	}
}
