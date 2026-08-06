// UEREMCP — Voxel interior-terrain domain (dungeon open-world ops).
//
// Depends on VoxelFree (module "Voxel"). Water is optional at compile time.

using UnrealBuildTool;

public class UeremcpVoxel : ModuleRules
{
	public UeremcpVoxel(ReadOnlyTargetRules Target) : base(Target)
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

			"Voxel",

			"UeremcpCore",
			"UeremcpSecurity",
			"ToolsetRegistry",
			"UeremcpProtocol",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("Water");
		}
	}
}
