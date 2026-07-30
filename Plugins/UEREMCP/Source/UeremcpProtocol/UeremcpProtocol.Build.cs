// UEREMCP — protocol module (ADR-0003 envelope, ADR-0004 graph, ADR-0006 revisions).
//
// Deliberately depends on NEITHER ToolsetRegistry NOR ModelContextProtocol. Envelope
// handling must be testable outside the editor (RB-14 q10) and must survive Epic's
// experimental APIs churning (ADR-0001, R-02). If you find yourself wanting a
// ToolsetRegistry include here, the layering is wrong — put it in UeremcpCore.

using UnrealBuildTool;

public class UeremcpProtocol : ModuleRules
{
	public UeremcpProtocol(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
		});
	}
}
