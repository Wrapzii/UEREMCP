// UEREMCP — specification.features resolution (WS-08).

#include "UeremcpMaterialFeatures.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Misc/Crc.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	static bool FeatureSetContains(const TSet<FString>& Set, const FString& Token)
	{
		return Set.Contains(Token);
	}
}

void UeremcpMaterialFeatures::ParseFeaturesFromSpec(
	const TSharedPtr<FJsonObject>& Spec,
	TArray<FString>& OutFeatures)
{
	OutFeatures.Reset();
	const TArray<TSharedPtr<FJsonValue>>* FeatureArray = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("features"), FeatureArray) && FeatureArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *FeatureArray)
		{
			FString Feature;
			if (Value.IsValid() && Value->TryGetString(Feature) && !Feature.IsEmpty())
			{
				OutFeatures.Add(Feature);
			}
		}
	}
}

TArray<FString> UeremcpMaterialFeatures::DefaultFeaturesForPurpose(const FString& Purpose)
{
	if (Purpose.Equals(TEXT("elemental_projectile_trail"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_ribbon_trail"), ESearchCase::CaseSensitive))
	{
		return {
			TEXT("panning_textures"),
			TEXT("erosion"),
			TEXT("depth_fade"),
			TEXT("dynamic_color"),
		};
	}
	if (Purpose.Equals(TEXT("elemental_projectile_core"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_core"), ESearchCase::CaseSensitive))
	{
		return {
			TEXT("radial_falloff"),
			TEXT("animated_noise"),
			TEXT("fresnel"),
			TEXT("dynamic_color"),
			TEXT("dynamic_intensity"),
		};
	}
	return {};
}

TArray<FString> UeremcpMaterialFeatures::ResolveFeaturesForPurpose(
	const FString& Purpose,
	const TArray<FString>& Requested)
{
	if (Requested.Num() > 0)
	{
		return Requested;
	}
	return DefaultFeaturesForPurpose(Purpose);
}

FString UeremcpMaterialFeatures::ComputeFeatureSignature(const TArray<FString>& Features)
{
	TArray<FString> Sorted = Features;
	Sorted.Sort();
	const FString Joined = FString::Join(Sorted, TEXT(","));
	const uint32 Crc = FCrc::StrCrc32(*Joined);
	return FString::Printf(TEXT("%08X"), Crc);
}

bool UeremcpMaterialFeatures::IsTrailPurpose(const FString& Purpose)
{
	return Purpose.Equals(TEXT("elemental_projectile_trail"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_ribbon_trail"), ESearchCase::CaseSensitive);
}

FString UeremcpMaterialFeatures::MasterBaseAssetName(const FString& Purpose)
{
	if (IsTrailPurpose(Purpose))
	{
		return TEXT("M_Ueremcp_ProjTrail");
	}
	if (Purpose.Equals(TEXT("elemental_projectile_core"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_core"), ESearchCase::CaseSensitive))
	{
		return TEXT("M_Ueremcp_ProjCore");
	}
	return TEXT("M_Ueremcp_VFX_Generic");
}

FString UeremcpMaterialFeatures::ResolveMasterPackagePath(
	const FString& Purpose,
	const TArray<FString>& Features)
{
	const FString Base = MasterBaseAssetName(Purpose);
	const FString Signature = ComputeFeatureSignature(Features);
	const FString AssetName = FString::Printf(TEXT("%s_%s"), *Base, *Signature);
	return UeremcpMaterialPaths::JoinPackagePath(UeremcpMaterialPaths::MastersFolder, AssetName);
}

bool UeremcpMaterialFeatures::IsImplementedFeature(const FString& Feature)
{
	static const TSet<FString> Implemented = {
		TEXT("radial_falloff"),
		TEXT("animated_noise"),
		TEXT("fresnel"),
		TEXT("dynamic_color"),
		TEXT("dynamic_intensity"),
		TEXT("panning_textures"),
		TEXT("erosion"),
		TEXT("depth_fade"),
	};
	return Implemented.Contains(Feature);
}

TArray<FString> UeremcpMaterialFeatures::FindUnimplementedFeatures(const TArray<FString>& Features)
{
	TArray<FString> Unimplemented;
	for (const FString& Feature : Features)
	{
		if (!IsImplementedFeature(Feature))
		{
			Unimplemented.Add(Feature);
		}
	}
	return Unimplemented;
}

bool UeremcpMaterialFeatures::VerifyFeatureGraph(
	UMaterial* Material,
	const TArray<FString>& Features,
	FFeatureGraphVerifyResult& OutResult)
{
	if (!Material)
	{
		return false;
	}

	OutResult.bEmissiveConnected =
		UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_EmissiveColor) != nullptr;
	OutResult.bOpacityConnected =
		UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_Opacity) != nullptr;

	TArray<const UMaterialExpressionSphereMask*> SphereMasks;
	Material->GetAllExpressionsOfType(SphereMasks);
	TArray<const UMaterialExpressionNoise*> Noises;
	Material->GetAllExpressionsOfType(Noises);
	TArray<const UMaterialExpressionFresnel*> Fresnels;
	Material->GetAllExpressionsOfType(Fresnels);
	TArray<const UMaterialExpressionPanner*> Panners;
	Material->GetAllExpressionsOfType(Panners);
	TArray<const UMaterialExpressionDepthFade*> DepthFades;
	Material->GetAllExpressionsOfType(DepthFades);
	TArray<const UMaterialExpressionOneMinus*> OneMinusNodes;
	Material->GetAllExpressionsOfType(OneMinusNodes);

	const TSet<FString> FeatureSet(Features);
	OutResult.FeatureWired.Add(TEXT("radial_falloff"), FeatureSetContains(FeatureSet, TEXT("radial_falloff")) ? SphereMasks.Num() > 0 : true);
	OutResult.FeatureWired.Add(TEXT("animated_noise"), FeatureSetContains(FeatureSet, TEXT("animated_noise")) ? Noises.Num() > 0 : true);
	OutResult.FeatureWired.Add(TEXT("fresnel"), FeatureSetContains(FeatureSet, TEXT("fresnel")) ? Fresnels.Num() > 0 : true);
	OutResult.FeatureWired.Add(TEXT("panning_textures"), FeatureSetContains(FeatureSet, TEXT("panning_textures")) ? Panners.Num() > 0 : true);
	OutResult.FeatureWired.Add(TEXT("depth_fade"), FeatureSetContains(FeatureSet, TEXT("depth_fade")) ? DepthFades.Num() > 0 && OutResult.bOpacityConnected : true);
	OutResult.FeatureWired.Add(TEXT("erosion"), FeatureSetContains(FeatureSet, TEXT("erosion")) ? OneMinusNodes.Num() > 0 : true);
	OutResult.FeatureWired.Add(TEXT("dynamic_color"), true);
	OutResult.FeatureWired.Add(TEXT("dynamic_intensity"), true);

	if (!OutResult.bEmissiveConnected)
	{
		return false;
	}

	for (const FString& Feature : Features)
	{
		if (!IsImplementedFeature(Feature))
		{
			continue;
		}
		const bool* bWired = OutResult.FeatureWired.Find(Feature);
		if (bWired && !(*bWired))
		{
			return false;
		}
	}

	if (FeatureSetContains(FeatureSet, TEXT("depth_fade")) && !OutResult.bOpacityConnected)
	{
		return false;
	}

	return true;
}
