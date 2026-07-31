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
			"RenderCore",
			"LevelEditor",

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
			"Niagara",

			// Domain Material E3/E4 gates.
			"UeremcpMaterial",

			// E1 restart seed creates and validates POC C gameplay-bound variations.
			"UeremcpTemplates",

			// WS-06 MutatingDispatch adapter regression (skips until handoff header lands).
			"UeremcpBlueprint",

			// Blueprint compile probe (ADR-0005 open q4).
			"BlueprintGraph",
			"Kismet",
			"KismetCompiler",
		});

		// D5 Pattern B proof needs the RE game module. BuildPlugin HostProject has no
		// RE sources — keep Validation packable without the live RE project.
		bool bHasRE = false;
		if (Target.ProjectFile != null)
		{
			string reBuildCs = System.IO.Path.Combine(
				Target.ProjectFile.Directory.FullName, "Source", "RE", "RE.Build.cs");
			bHasRE = System.IO.File.Exists(reBuildCs);
		}
		if (bHasRE)
		{
			PrivateDependencyModuleNames.Add("RE");
			PublicDefinitions.Add("UEREMCP_WITH_RE=1");
		}
		else
		{
			PublicDefinitions.Add("UEREMCP_WITH_RE=0");
		}
	}
}
