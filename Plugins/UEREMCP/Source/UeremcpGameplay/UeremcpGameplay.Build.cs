// UEREMCP — RE-native gameplay orchestration module (WS-09).

using UnrealBuildTool;

public class UeremcpGameplay : ModuleRules
{
	public UeremcpGameplay(ReadOnlyTargetRules Target) : base(Target)
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
			"Json",
			"JsonUtilities",

			"UeremcpProtocol",
			"UeremcpSecurity",

			// Agent-facing declaration only.
			// [VERIFIED: ToolsetDefinition.h:142-158 in docs/GROUNDED_FACTS.md §2.1]
			"ToolsetRegistry",
		});
	}
}
