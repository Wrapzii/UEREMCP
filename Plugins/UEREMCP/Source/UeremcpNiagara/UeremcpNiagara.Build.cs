// UEREMCP — Niagara domain module (WS-07).
//
// Inspect/create orchestrator over UNiagaraExternalEditUtilities (Epic NiagaraToolsets
// composition surface). Primitives stay internal (ADR-0002 rule 5).

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
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"Projects",

			"UeremcpCore",
			// Epic's tool-declaration layer (UToolsetDefinition / AICallable). ADR-0002.
			"ToolsetRegistry",

			"UeremcpProtocol",

			// WS-08 create_vfx_material export for inline materials.<role>.create_spec (probe paths only).
			"UeremcpMaterial",

			// Epic Niagara editor surface — used when inspect/create is implemented.
			// NiagaraToolsets plugin must be enabled in the target project.
			"Niagara",
			"NiagaraEditor",
		});
	}
}
