// UEREMCP — remaining non-environment coverage domains (WS-01).
// Audio / networking validation / world-partition inspect+repair.

using UnrealBuildTool;

public class UeremcpSystems : ModuleRules
{
	public UeremcpSystems(ReadOnlyTargetRules Target) : base(Target)
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
			"Kismet",

			// SoundCue / SoundAttenuation factories
			// [VERIFIED: Engine/Source/Editor/AudioEditor/Classes/Factories/SoundCueFactoryNew.h]
			// [VERIFIED: Engine/Source/Editor/AudioEditor/Classes/Factories/SoundAttenuationFactory.h]
			"AudioEditor",

			"UeremcpCore",
			"UeremcpSecurity",
			"ToolsetRegistry",
			"UeremcpProtocol",
		});
	}
}
