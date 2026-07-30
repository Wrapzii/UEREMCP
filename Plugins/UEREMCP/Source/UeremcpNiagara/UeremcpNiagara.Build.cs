// UEREMCP — Niagara domain module (WS-07).
//
// Wave 2 scaffold: thin toolset over Epic NiagaraToolsets + execute_tool_script
// batching. Primitives stay internal (ADR-0002 rule 5).

using UnrealBuildTool;

public class UeremcpNiagara : ModuleRules
{
	public UeremcpNiagara(ReadOnlyTargetRules Target) : base(Target)
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

			"UeremcpCore",
			"UeremcpProtocol",

			// Epic Niagara editor surface — used when inspect/create is implemented.
			// NiagaraToolsets plugin must be enabled in the target project.
			"Niagara",
			"NiagaraEditor",
		});
	}
}
