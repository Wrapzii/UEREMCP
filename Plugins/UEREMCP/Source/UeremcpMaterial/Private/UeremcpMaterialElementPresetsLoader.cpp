// UEREMCP — runtime loader for element_presets.v1.json (WS-08).
//
// Substrate: FFileHelper::LoadFileToString [VERIFIED: FileHelper.h]
// Path resolution pattern mirrors UeremcpTemplates::ResolveTemplatesDirectory.

#include "UeremcpMaterialElementPresetsLoader.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FPresetsCache
	{
		bool bInitialized = false;
		bool bLoadedFromJson = false;
		FString LoadedPath;
		TMap<FString, FUeremcpMaterialParameterSet> Elements;
		TMap<FString, TArray<FString>> PurposeDefaultFeatures;
	};

	static FPresetsCache& GetCache()
	{
		static FPresetsCache Cache;
		return Cache;
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

	static bool ParseElementEntry(const TSharedPtr<FJsonObject>& ElementObj, FUeremcpMaterialParameterSet& OutPreset)
	{
		if (!ElementObj.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
		if (!ElementObj->TryGetArrayField(TEXT("particle_color"), ColorArray) ||
			!ColorArray ||
			!ParseLinearColorArray(*ColorArray, OutPreset.ParticleColor))
		{
			return false;
		}
		if (!ElementObj->TryGetArrayField(TEXT("color_secondary"), ColorArray) ||
			!ColorArray ||
			!ParseLinearColorArray(*ColorArray, OutPreset.ColorSecondary))
		{
			return false;
		}

		double Number = 0.0;
		if (!ElementObj->TryGetNumberField(TEXT("emissive_scale"), Number))
		{
			return false;
		}
		OutPreset.EmissiveScale = static_cast<float>(Number);
		if (!ElementObj->TryGetNumberField(TEXT("flow_speed"), Number))
		{
			return false;
		}
		OutPreset.FlowSpeed = static_cast<float>(Number);
		if (!ElementObj->TryGetNumberField(TEXT("turbulence"), Number))
		{
			return false;
		}
		OutPreset.Turbulence = static_cast<float>(Number);
		if (!ElementObj->TryGetNumberField(TEXT("soft_edge"), Number))
		{
			return false;
		}
		OutPreset.SoftEdge = static_cast<float>(Number);
		if (!ElementObj->TryGetNumberField(TEXT("depth_fade"), Number))
		{
			return false;
		}
		OutPreset.DepthFade = static_cast<float>(Number);
		return true;
	}

	static FString TryResolveExistingFile(const FString& Candidate)
	{
		const FString Normalized = FPaths::ConvertRelativePathToFull(Candidate);
		return FPaths::FileExists(Normalized) ? Normalized : FString();
	}

	static void EnsureLoaded()
	{
		FPresetsCache& Cache = GetCache();
		if (Cache.bInitialized)
		{
			return;
		}
		Cache.bInitialized = true;

		const FString Path = UeremcpMaterialElementPresetsLoader::ResolvePresetsJsonPath();
		if (Path.IsEmpty())
		{
			return;
		}

		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *Path))
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>* ElementsObj = nullptr;
		if (Root->TryGetObjectField(TEXT("elements"), ElementsObj) && ElementsObj && ElementsObj->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ElementsObj)->Values)
			{
				const TSharedPtr<FJsonObject> ElementObj = Pair.Value->AsObject();
				FUeremcpMaterialParameterSet Preset;
				if (ParseElementEntry(ElementObj, Preset))
				{
					Cache.Elements.Add(Pair.Key.ToLower(), Preset);
				}
			}
		}

		const TSharedPtr<FJsonObject>* PurposeFeaturesObj = nullptr;
		if (Root->TryGetObjectField(TEXT("purpose_default_features"), PurposeFeaturesObj) &&
			PurposeFeaturesObj &&
			PurposeFeaturesObj->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PurposeFeaturesObj)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* FeatureArray = nullptr;
				if (Pair.Value->TryGetArray(FeatureArray) && FeatureArray)
				{
					TArray<FString> Features;
					for (const TSharedPtr<FJsonValue>& Value : *FeatureArray)
					{
						FString Feature;
						if (Value.IsValid() && Value->TryGetString(Feature) && !Feature.IsEmpty())
						{
							Features.Add(Feature);
						}
					}
					if (Features.Num() > 0)
					{
						Cache.PurposeDefaultFeatures.Add(Pair.Key, Features);
					}
				}
			}
		}

		if (Cache.Elements.Num() > 0)
		{
			Cache.bLoadedFromJson = true;
			Cache.LoadedPath = Path;
		}
	}
}

FString UeremcpMaterialElementPresetsLoader::ResolvePresetsJsonPath()
{
	const TCHAR* RelativeRepo = TEXT("../../schemas/domains/materials/element_presets.v1.json");
	const TCHAR* RelativeBundled = TEXT("Resources/Materials/element_presets.v1.json");

	const auto TryPlugin = [&](const FString& PluginName) -> FString
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			return FString();
		}
		const FString BaseDir = Plugin->GetBaseDir();
		if (const FString RepoPath = TryResolveExistingFile(FPaths::Combine(BaseDir, RelativeRepo)); !RepoPath.IsEmpty())
		{
			return RepoPath;
		}
		if (const FString BundledPath = TryResolveExistingFile(FPaths::Combine(BaseDir, RelativeBundled)); !BundledPath.IsEmpty())
		{
			return BundledPath;
		}
		return FString();
	};

	if (const FString FromPlugin = TryPlugin(TEXT("UEREMCP")); !FromPlugin.IsEmpty())
	{
		return FromPlugin;
	}

	const TArray<FString> FallbackCandidates = {
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UEREMCP"), RelativeRepo),
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UEREMCP"), RelativeBundled),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/UEREMCP"), RelativeRepo),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/UEREMCP"), RelativeBundled),
	};

	for (const FString& Candidate : FallbackCandidates)
	{
		if (const FString Resolved = TryResolveExistingFile(Candidate); !Resolved.IsEmpty())
		{
			return Resolved;
		}
	}

	return FString();
}

bool UeremcpMaterialElementPresetsLoader::IsLoadedFromJson()
{
	EnsureLoaded();
	return GetCache().bLoadedFromJson;
}

FString UeremcpMaterialElementPresetsLoader::GetLoadedPath()
{
	EnsureLoaded();
	return GetCache().LoadedPath;
}

bool UeremcpMaterialElementPresetsLoader::TryGetElementDefaults(
	const FString& Element,
	FUeremcpMaterialParameterSet& OutPreset)
{
	EnsureLoaded();
	const FPresetsCache& Cache = GetCache();
	if (!Cache.bLoadedFromJson)
	{
		return false;
	}

	const FString Key = Element.ToLower();
	if (const FUeremcpMaterialParameterSet* Found = Cache.Elements.Find(Key))
	{
		OutPreset = *Found;
		return true;
	}
	return false;
}

bool UeremcpMaterialElementPresetsLoader::TryGetPurposeDefaultFeatures(
	const FString& Purpose,
	TArray<FString>& OutFeatures)
{
	EnsureLoaded();
	const FPresetsCache& Cache = GetCache();
	if (!Cache.bLoadedFromJson)
	{
		return false;
	}

	if (const TArray<FString>* Found = Cache.PurposeDefaultFeatures.Find(Purpose))
	{
		OutFeatures = *Found;
		return true;
	}
	return false;
}
