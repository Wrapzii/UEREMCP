// UEREMCP — goal-level Unreal agent interface.

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

			// Epic's tool-declaration layer. ADR-0002: we register into this rather
			// than standing up our own MCP server.
			"ToolsetRegistry",

			// UEREMCP envelope parse/serialise/validate. ADR-0003.
			"UeremcpProtocol",
		});

		// NOTE (RB-03 q10): ToolsetRegistry keeps RunOnMainThread.h, JsonSchema.h and
		// ValueOrErrorFuture.h under Private/. If an out-of-tree plugin cannot reach
		// them, we need public equivalents in this module. Do NOT add a private-include
		// path hack without recording the decision in docs/proposals/ first — that is
		// exactly the kind of coupling ADR-0001's churn mitigation exists to avoid.
	}
}
