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

			// Epic's tool-declaration layer. ADR-0002.
			// [VERIFIED: $TR/ToolsetRegistry.uplugin — Editor module]
			"ToolsetRegistry",
		});

		// UeremcpProtocol is registered in .uplugin and now links as its own module.
		// Ping/Echo still use Private/UeremcpMinimalEnvelope.h until Core is rewired
		// to FUeremcpEnvelope (see docs/proposals/ws-03-protocol-module-blocker.md).
		//
		// RB-03 q10: ToolsetRegistry Private headers are NOT on the public include path.
		// [VERIFIED: ToolsetRegistry.Build.cs PublicIncludePaths empty]
		// Public equivalents: Async(TaskGraphMainThread), TValueOrError, FJsonSchemaGenerator,
		// UToolsetRegistry::RegisterToolsetClass.
	}
}