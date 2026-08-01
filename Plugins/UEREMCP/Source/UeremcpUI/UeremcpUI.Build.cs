// UEREMCP — UI / presentation domain (MMO overlays, WBP goal APIs).

using UnrealBuildTool;

public class UeremcpUI : ModuleRules
{
	public UeremcpUI(ReadOnlyTargetRules Target) : base(Target)
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
			"AssetRegistry",
			"EditorScriptingUtilities",
			"LevelEditor",
			"Json",
			"JsonUtilities",
			"Projects",
			"Slate",
			"SlateCore",
			"UMG",
			"UMGEditor",
			"Kismet",
			"KismetCompiler",
			"RenderCore",
			"RHI",
			"ImageWrapper",

			"UeremcpCore",
			"UeremcpSecurity",
			"ToolsetRegistry",
			"UeremcpProtocol",
		});
	}
}
