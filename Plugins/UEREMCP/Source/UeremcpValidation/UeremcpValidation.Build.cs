// UEREMCP — validation & testing module (WS-11).
//
// Editor-only. Depends on ToolsetRegistry for FGlobalSandbox (ADR-0005) and on
// UnrealEd for asset create/save/registry assertions. Does NOT depend on
// ModelContextProtocol — tests must be runnable via Automation / UnrealEditor-Cmd
// without an MCP client (RB-14).

using UnrealBuildTool;

public class UeremcpValidation : ModuleRules
{
	public UeremcpValidation(ReadOnlyTargetRules Target) : base(Target)
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
			"AssetRegistry",
			"AssetTools",
			"EditorScriptingUtilities",
			"Json",
			"JsonUtilities",
			"Projects",

			// ADR-0005 outer layer. [VERIFIED: $TR/.../Public/ToolsetRegistry/SandboxLibrary.h]
			"ToolsetRegistry",

			// FileSandboxCore types (FSandboxedFileChangeInfo, ESandboxFileChange).
			// [VERIFIED: $FS/.../Public/Types/SandboxedFileChangeInfo.h]
			"FileSandboxCore",

			// ADR-0006 protocol primitives (idempotency store, content_hash, envelope).
			"UeremcpProtocol",

			// Blueprint dispatch regression consumes FUeremcpMutatingDispatch via
			// UeremcpBlueprintMutatingGate's public header.
			"UeremcpCore",
			"UeremcpSecurity",

			// WS-07 POC B editor gate calls the goal-level create tool directly.
			"UeremcpNiagara",

			// WS-06 MutatingDispatch adapter regression (skips until handoff header lands).
			"UeremcpBlueprint",

			// Blueprint compile probe (ADR-0005 open q4).
			"BlueprintGraph",
			"Kismet",
			"KismetCompiler",
		});
	}
}
