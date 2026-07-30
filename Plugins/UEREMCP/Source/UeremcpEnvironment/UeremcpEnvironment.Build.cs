// UEREMCP — Environment / world domain module (WS-01, BACKLOG Tier 4–5).
//
// Goal-level BuildEnvironment / InspectEnvironment / ValidateEnvironment.
// Landscape Import API: [VERIFIED: LandscapeProxy.h:1418-1420]
// Water river actor: [VERIFIED: WaterBodyRiverActor.h:28]
// GeometryScripting + Water enabled in RE.uproject (BACKLOG 0.1/0.2).

using UnrealBuildTool;

public class UeremcpEnvironment : ModuleRules
{
	public UeremcpEnvironment(ReadOnlyTargetRules Target) : base(Target)
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
			"AssetTools",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"Projects",
			"Landscape",
			"LandscapeEditor",
			"Foliage",
			"Niagara",

			"UeremcpCore",
			"UeremcpSecurity",
			"ToolsetRegistry",
			"UeremcpProtocol",
		});

		// Water is optional at compile time when the plugin is present.
		// [VERIFIED-RUNTIME: Water plugin enabled in RE; IsEnabled=true 2026-07-30]
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("Water");
		}
	}
}
