// UEREMCP — element preset defaults (WS-08).

#include "UeremcpMaterialElementPresets.h"

#include "Dom/JsonObject.h"
#include "UeremcpMaterialElementPresetsLoader.h"
#include "UeremcpMaterialFeatures.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	static FUeremcpMaterialParameterSet MakePreset(
		const FLinearColor& ParticleColor,
		const FLinearColor& ColorSecondary,
		float EmissiveScale,
		float FlowSpeed,
		float Turbulence,
		float SoftEdge,
		float DepthFade)
	{
		FUeremcpMaterialParameterSet Preset;
		Preset.ParticleColor = ParticleColor;
		Preset.ColorSecondary = ColorSecondary;
		Preset.EmissiveScale = EmissiveScale;
		Preset.FlowSpeed = FlowSpeed;
		Preset.Turbulence = Turbulence;
		Preset.SoftEdge = SoftEdge;
		Preset.DepthFade = DepthFade;
		return Preset;
	}

	static bool ParseLinearColorArray(const TArray<TSharedPtr<FJsonValue>>& Values, FLinearColor& OutColor)
	{
		if (Values.Num() < 3)
		{
			return false;
		}
		OutColor.R = static_cast<float>(Values[0]->AsNumber());
		OutColor.G = static_cast<float>(Values[1]->AsNumber());
		OutColor.B = static_cast<float>(Values[2]->AsNumber());
		OutColor.A = Values.Num() > 3 ? static_cast<float>(Values[3]->AsNumber()) : 1.0f;
		return true;
	}
}

FString UeremcpMaterialElementPresets::ResolveMasterAssetName(const FString& Purpose)
{
	return UeremcpMaterialFeatures::MasterBaseAssetName(Purpose);
}

FString UeremcpMaterialElementPresets::ResolveMasterPackagePath(
	const FString& Purpose,
	const TArray<FString>& Features)
{
	return UeremcpMaterialFeatures::ResolveMasterPackagePath(Purpose, Features);
}

bool UeremcpMaterialElementPresets::GetElementDefaults(const FString& Element, FUeremcpMaterialParameterSet& OutPreset)
{
	if (UeremcpMaterialElementPresetsLoader::TryGetElementDefaults(Element, OutPreset))
	{
		return true;
	}

	const FString Key = Element.ToLower();
	if (Key == TEXT("fire"))
	{
		OutPreset = MakePreset(
			FLinearColor(1.0f, 0.35f, 0.05f, 1.0f),
			FLinearColor(1.0f, 0.8f, 0.2f, 1.0f),
			8.0f, 0.3f, 0.8f, 0.85f, 120.0f);
		return true;
	}
	if (Key == TEXT("water"))
	{
		OutPreset = MakePreset(
			FLinearColor(0.1f, 0.4f, 0.8f, 1.0f),
			FLinearColor(0.8f, 0.9f, 1.0f, 1.0f),
			4.0f, 0.5f, 0.4f, 0.7f, 150.0f);
		return true;
	}
	if (Key == TEXT("wind"))
	{
		OutPreset = MakePreset(
			FLinearColor(0.7f, 0.85f, 1.0f, 1.0f),
			FLinearColor(0.9f, 0.95f, 1.0f, 1.0f),
			3.0f, 1.2f, 0.9f, 0.6f, 100.0f);
		return true;
	}
	if (Key == TEXT("earth"))
	{
		OutPreset = MakePreset(
			FLinearColor(0.3f, 0.25f, 0.2f, 1.0f),
			FLinearColor(0.5f, 0.45f, 0.35f, 1.0f),
			2.0f, 0.1f, 0.3f, 0.9f, 80.0f);
		return true;
	}
	if (Key == TEXT("ice"))
	{
		OutPreset = MakePreset(
			FLinearColor(0.6f, 0.85f, 1.0f, 1.0f),
			FLinearColor(0.9f, 0.95f, 1.0f, 1.0f),
			6.0f, 0.4f, 0.5f, 0.75f, 130.0f);
		return true;
	}
	return false;
}

void UeremcpMaterialElementPresets::ApplyModifiers(
	const TArray<FString>& Modifiers,
	const FString& Purpose,
	FUeremcpMaterialParameterSet& InOutPreset)
{
	const bool bTrailPurpose =
		Purpose.Equals(TEXT("elemental_projectile_trail"), ESearchCase::CaseSensitive) ||
		Purpose.Equals(TEXT("fireball_ribbon_trail"), ESearchCase::CaseSensitive);

	for (const FString& Modifier : Modifiers)
	{
		if (Modifier.Equals(TEXT("boost_impact"), ESearchCase::CaseSensitive))
		{
			InOutPreset.EmissiveScale = FMath::Max(InOutPreset.EmissiveScale * 1.5f, 12.0f);
		}
		else if (Modifier.Equals(TEXT("crystalline_fragments"), ESearchCase::CaseSensitive))
		{
			InOutPreset.Turbulence = FMath::Max(InOutPreset.Turbulence, 0.85f);
			InOutPreset.ColorSecondary = FLinearColor(
				FMath::Min(InOutPreset.ColorSecondary.R + 0.1f, 1.0f),
				FMath::Min(InOutPreset.ColorSecondary.G + 0.05f, 1.0f),
				FMath::Min(InOutPreset.ColorSecondary.B + 0.05f, 1.0f),
				InOutPreset.ColorSecondary.A);
		}
		else if (Modifier.Equals(TEXT("reduce_trail_persistence"), ESearchCase::CaseSensitive) && bTrailPurpose)
		{
			InOutPreset.SoftEdge = FMath::Max(InOutPreset.SoftEdge - 0.15f, 0.2f);
			InOutPreset.EmissiveScale *= 0.75f;
		}
		// preserve_networking — no material-side effect (ADR-0008 modifier; Niagara owns networking).
	}
}

void UeremcpMaterialElementPresets::MergeParameterOverrides(
	const TSharedPtr<FJsonObject>& Overrides,
	FUeremcpMaterialParameterSet& InOutPreset)
{
	if (!Overrides.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
	if (Overrides->TryGetArrayField(TEXT("ParticleColor"), ColorArray) && ColorArray)
	{
		ParseLinearColorArray(*ColorArray, InOutPreset.ParticleColor);
	}
	if (Overrides->TryGetArrayField(TEXT("ColorSecondary"), ColorArray) && ColorArray)
	{
		ParseLinearColorArray(*ColorArray, InOutPreset.ColorSecondary);
	}

	double Number = 0.0;
	if (Overrides->TryGetNumberField(TEXT("EmissiveScale"), Number))
	{
		InOutPreset.EmissiveScale = static_cast<float>(Number);
	}
	if (Overrides->TryGetNumberField(TEXT("FlowSpeed"), Number))
	{
		InOutPreset.FlowSpeed = static_cast<float>(Number);
	}
	if (Overrides->TryGetNumberField(TEXT("Turbulence"), Number))
	{
		InOutPreset.Turbulence = static_cast<float>(Number);
	}
	if (Overrides->TryGetNumberField(TEXT("SoftEdge"), Number))
	{
		InOutPreset.SoftEdge = static_cast<float>(Number);
	}
	if (Overrides->TryGetNumberField(TEXT("DepthFade"), Number))
	{
		InOutPreset.DepthFade = static_cast<float>(Number);
	}
	if (Overrides->TryGetNumberField(TEXT("DissolveAmount"), Number))
	{
		InOutPreset.DissolveAmount = static_cast<float>(Number);
	}
}
