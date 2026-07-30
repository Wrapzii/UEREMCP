// UEREMCP — Animation domain module (WS-10).

using UnrealBuildTool;

public class UeremcpAnimation : ModuleRules
{
	public UeremcpAnimation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AnimGraph",
			"AnimationBlueprintLibrary",
			"ToolsetRegistry",
			"UeremcpBlueprint",
			"UeremcpProtocol",
		});
	}
}
