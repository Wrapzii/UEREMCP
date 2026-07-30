// UEREMCP — template & pattern library (ADR-0008). Owner: WS-15.

using UnrealBuildTool;

public class UeremcpTemplates : ModuleRules
{
	public UeremcpTemplates(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Json",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"JsonUtilities",
			"Projects",
			"ToolsetRegistry",
			"UeremcpProtocol",
		});
	}
}
