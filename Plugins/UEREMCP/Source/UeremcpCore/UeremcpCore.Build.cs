// UEREMCP — goal-level Unreal agent interface (WS-03 / UeremcpCore).

using UnrealBuildTool;

public class UeremcpCore : ModuleRules
{
	public UeremcpCore(ReadOnlyTargetRules Target) : base(Target)
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

			// ADR-0003 envelope parse/serialize (WS-05). Reference Ping/Echo route here.
			"UeremcpProtocol",

			// Epic's tool-declaration layer. ADR-0002.
			// [VERIFIED: $TR/ToolsetRegistry.uplugin — Editor module]
			"ToolsetRegistry",
		});
		//
		// RB-03 q10: ToolsetRegistry Private headers are NOT on the public include path.
		// [VERIFIED: ToolsetRegistry.Build.cs PublicIncludePaths empty]
		// Public equivalents: Async(TaskGraphMainThread), TValueOrError, FJsonSchemaGenerator,
		// UToolsetRegistry::RegisterToolsetClass.
	}
}