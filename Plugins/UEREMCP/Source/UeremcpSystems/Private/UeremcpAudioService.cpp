#include "UeremcpAudioService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/SoundAttenuationFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UeremcpSystemsHelpers.h"

namespace
{
	FString HashStrings(const TArray<FString>& Parts)
	{
		FSHA1 Sha;
		for (const FString& Part : Parts)
		{
			const FTCHARToUTF8 Utf8(*Part);
			Sha.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		}
		Sha.Final();
		uint8 Digest[FSHA1::DigestSize];
		Sha.GetHash(Digest);
		return BytesToHex(Digest, FSHA1::DigestSize).ToLower();
	}

	void CollectWavePathsFromCue(const USoundCue* Cue, TArray<FString>& OutPaths)
	{
		if (!Cue)
		{
			return;
		}
#if WITH_EDITORONLY_DATA
		for (const USoundNode* Node : Cue->AllNodes)
		{
			const USoundNodeWavePlayer* WavePlayer = Cast<USoundNodeWavePlayer>(Node);
			if (!WavePlayer)
			{
				continue;
			}
			if (const USoundWave* Wave = WavePlayer->GetSoundWave())
			{
				OutPaths.AddUnique(Wave->GetPathName());
			}
		}
#else
		if (const USoundNodeWavePlayer* WavePlayer = Cast<USoundNodeWavePlayer>(Cue->FirstNode))
		{
			if (const USoundWave* Wave = WavePlayer->GetSoundWave())
			{
				OutPaths.AddUnique(Wave->GetPathName());
			}
		}
#endif
	}
	bool EnsureParentPackage(const FString& PackagePath, FString& OutError)
	{
		const FString Parent = FPackageName::GetLongPackagePath(PackagePath);
		if (Parent.IsEmpty())
		{
			OutError = TEXT("target.asset_path parent package path is empty");
			return false;
		}
		return true;
	}

	bool SaveAssetPackage(UObject* Asset, FString& OutError)
	{
		if (!Asset)
		{
			OutError = TEXT("cannot save null asset");
			return false;
		}
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs))
		{
			OutError = FString::Printf(TEXT("SavePackage failed for '%s'"), *Package->GetName());
			return false;
		}
		return true;
	}
}

bool FUeremcpAudioService::ParseCreateSpec(
	const TSharedPtr<FJsonObject>& Specification,
	const FString& TargetAssetPath,
	FUeremcpAudioCuePlan& OutPlan,
	FString& OutError)
{
	OutPlan = FUeremcpAudioCuePlan();
	if (!Specification.IsValid())
	{
		OutError = TEXT("create_audio_cue requires specification");
		return false;
	}
	if (!UeremcpSystems::IsScratchPath(TargetAssetPath))
	{
		OutError = TEXT("create_audio_cue writes only under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Waves = nullptr;
	if (!Specification->TryGetArrayField(TEXT("sound_waves"), Waves) || !Waves || Waves->Num() == 0)
	{
		OutError = TEXT("specification.sound_waves must be a non-empty array of SoundWave paths");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Waves)
	{
		FString Path;
		if (!Value.IsValid() || !Value->TryGetString(Path) || Path.IsEmpty())
		{
			OutError = TEXT("specification.sound_waves entries must be non-empty strings");
			return false;
		}
		OutPlan.SoundWavePaths.Add(Path);
	}

	OutPlan.VolumeMultiplier = 1.0f;
	OutPlan.PitchMultiplier = 1.0f;
	if (Specification->HasTypedField<EJson::Number>(TEXT("volume_multiplier")))
	{
		OutPlan.VolumeMultiplier = static_cast<float>(Specification->GetNumberField(TEXT("volume_multiplier")));
	}
	if (Specification->HasTypedField<EJson::Number>(TEXT("pitch_multiplier")))
	{
		OutPlan.PitchMultiplier = static_cast<float>(Specification->GetNumberField(TEXT("pitch_multiplier")));
	}

	Specification->TryGetBoolField(TEXT("create_attenuation"), OutPlan.bCreateAttenuation);
	Specification->TryGetStringField(TEXT("attenuation_asset_path"), OutPlan.AttenuationAssetPath);
	Specification->TryGetStringField(TEXT("existing_attenuation_path"), OutPlan.ExistingAttenuationPath);

	if (OutPlan.bCreateAttenuation && OutPlan.AttenuationAssetPath.IsEmpty())
	{
		OutPlan.AttenuationAssetPath = TargetAssetPath + TEXT("_Attenuation");
	}
	if (OutPlan.bCreateAttenuation && !UeremcpSystems::IsScratchPath(OutPlan.AttenuationAssetPath))
	{
		OutError = TEXT("attenuation_asset_path must be under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/");
		return false;
	}
	return true;
}

bool FUeremcpAudioService::Inspect(
	const FString& AssetPath,
	FUeremcpAudioInspection& OutInspection,
	FString& OutError)
{
	OutInspection = FUeremcpAudioInspection();
	const FString ObjectPath = UeremcpSystems::ResolveObjectPath(AssetPath);
	UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (!Obj)
	{
		OutError = FString::Printf(TEXT("Failed to load audio asset '%s'"), *AssetPath);
		return false;
	}

	OutInspection.AssetPath = Obj->GetPathName();
	OutInspection.AssetClass = Obj->GetClass()->GetName();

	if (const USoundCue* Cue = Cast<USoundCue>(Obj))
	{
		OutInspection.VolumeMultiplier = Cue->VolumeMultiplier;
		OutInspection.PitchMultiplier = Cue->PitchMultiplier;
		if (Cue->AttenuationSettings)
		{
			OutInspection.AttenuationPath = Cue->AttenuationSettings->GetPathName();
		}
		CollectWavePathsFromCue(Cue, OutInspection.SoundWavePaths);
	}
	else if (const USoundWave* Wave = Cast<USoundWave>(Obj))
	{
		OutInspection.SoundWavePaths.Add(Wave->GetPathName());
		OutInspection.VolumeMultiplier = Wave->Volume;
		OutInspection.PitchMultiplier = Wave->Pitch;
	}
	else if (const USoundAttenuation* Attenuation = Cast<USoundAttenuation>(Obj))
	{
		OutInspection.AttenuationPath = Attenuation->GetPathName();
	}
	else
	{
		OutError = FString::Printf(
			TEXT("Target '%s' is %s; expected SoundCue, SoundWave, or SoundAttenuation"),
			*AssetPath,
			*OutInspection.AssetClass);
		return false;
	}

	TArray<FString> HashParts = OutInspection.SoundWavePaths;
	HashParts.Add(OutInspection.AssetClass);
	HashParts.Add(OutInspection.AttenuationPath);
	HashParts.Add(FString::SanitizeFloat(OutInspection.VolumeMultiplier));
	HashParts.Add(FString::SanitizeFloat(OutInspection.PitchMultiplier));
	OutInspection.ContentHash = HashStrings(HashParts);
	return true;
}

bool FUeremcpAudioService::ExecuteCreateAudioCue(
	const FUeremcpRequest& Request,
	const FUeremcpAudioCuePlan& Plan,
	bool bDryRun,
	FUeremcpResponse& OutResponse,
	FString& OutError)
{
	OutResponse.RequestId = Request.RequestId;
	OutResponse.UnderstoodAction = Request.Action;
	OutResponse.UnderstoodTarget = Request.TargetAssetPath;
	OutResponse.PrimaryAsset = Request.TargetAssetPath;
	UeremcpSystems::AddCommonCapabilityNotes(OutResponse.CapabilityNotes);
	OutResponse.CapabilityNotes.Add(
		TEXT("create_audio_cue uses USoundCueFactoryNew.InitialSoundWaves [VERIFIED: SoundCueFactoryNew.h:44-45]."));
	OutResponse.Metrics.McpRoundTrips = 1;

	for (const FString& WavePath : Plan.SoundWavePaths)
	{
		const FString ObjectPath = UeremcpSystems::ResolveObjectPath(WavePath);
		if (!StaticLoadObject(USoundWave::StaticClass(), nullptr, *ObjectPath))
		{
			OutError = FString::Printf(TEXT("SoundWave '%s' could not be loaded"), *WavePath);
			return false;
		}
	}

	if (bDryRun)
	{
		OutResponse.Status = TEXT("partially_completed");
		OutResponse.Summary = FString::Printf(
			TEXT("Dry-run create_audio_cue for '%s' with %d wave(s)%s. No assets written."),
			*Request.TargetAssetPath,
			Plan.SoundWavePaths.Num(),
			Plan.bCreateAttenuation ? TEXT(" + new attenuation") : TEXT(""));
		OutResponse.ExtraFields = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> WavesJson;
		for (const FString& WavePath : Plan.SoundWavePaths)
		{
			WavesJson.Add(MakeShared<FJsonValueString>(WavePath));
		}
		OutResponse.ExtraFields->SetArrayField(TEXT("planned_sound_waves"), WavesJson);
		OutResponse.ExtraFields->SetBoolField(TEXT("planned_create_attenuation"), Plan.bCreateAttenuation);
		OutResponse.Metrics.InternalOperations = Plan.SoundWavePaths.Num();
		return true;
	}

	FString ParentError;
	if (!EnsureParentPackage(Request.TargetAssetPath, ParentError))
	{
		OutError = ParentError;
		return false;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	USoundAttenuation* Attenuation = nullptr;
	if (!Plan.ExistingAttenuationPath.IsEmpty())
	{
		Attenuation = LoadObject<USoundAttenuation>(
			nullptr,
			*UeremcpSystems::ResolveObjectPath(Plan.ExistingAttenuationPath));
		if (!Attenuation)
		{
			OutError = FString::Printf(
				TEXT("existing_attenuation_path '%s' could not be loaded"),
				*Plan.ExistingAttenuationPath);
			return false;
		}
	}
	else if (Plan.bCreateAttenuation)
	{
		USoundAttenuationFactory* AttenuationFactory = NewObject<USoundAttenuationFactory>();
		const FString AttPackage = Plan.AttenuationAssetPath;
		const FString AttName = FPaths::GetBaseFilename(AttPackage);
		const FString AttPath = FPackageName::GetLongPackagePath(AttPackage);
		UObject* CreatedAtt = AssetTools.CreateAsset(AttName, AttPath, USoundAttenuation::StaticClass(), AttenuationFactory);
		Attenuation = Cast<USoundAttenuation>(CreatedAtt);
		if (!Attenuation)
		{
			OutError = TEXT("USoundAttenuationFactory failed to create attenuation asset");
			return false;
		}
		if (Request.bSave)
		{
			FString SaveError;
			if (!SaveAssetPackage(Attenuation, SaveError))
			{
				OutError = SaveError;
				return false;
			}
		}
	}

	USoundCueFactoryNew* CueFactory = NewObject<USoundCueFactoryNew>();
	CueFactory->InitialSoundWaves.Reset();
	for (const FString& WavePath : Plan.SoundWavePaths)
	{
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, *UeremcpSystems::ResolveObjectPath(WavePath));
		CueFactory->InitialSoundWaves.Add(Wave);
	}

	const FString CuePackage = Request.TargetAssetPath;
	const FString CueName = FPaths::GetBaseFilename(CuePackage);
	const FString CuePath = FPackageName::GetLongPackagePath(CuePackage);
	UObject* CreatedCue = AssetTools.CreateAsset(CueName, CuePath, USoundCue::StaticClass(), CueFactory);
	USoundCue* Cue = Cast<USoundCue>(CreatedCue);
	if (!Cue)
	{
		OutError = TEXT("USoundCueFactoryNew failed to create SoundCue");
		return false;
	}

	Cue->VolumeMultiplier = Plan.VolumeMultiplier;
	Cue->PitchMultiplier = Plan.PitchMultiplier;
	if (Attenuation)
	{
		Cue->AttenuationSettings = Attenuation;
	}
	Cue->MarkPackageDirty();

	if (Request.bSave)
	{
		FString SaveError;
		if (!SaveAssetPackage(Cue, SaveError))
		{
			OutError = SaveError;
			return false;
		}
	}

	FUeremcpAudioInspection Inspection;
	FString InspectError;
	if (!Inspect(Request.TargetAssetPath, Inspection, InspectError))
	{
		OutResponse.Status = TEXT("created_with_warnings");
		OutResponse.Summary = FString::Printf(
			TEXT("Created SoundCue '%s' but re-read failed: %s"),
			*Request.TargetAssetPath,
			*InspectError);
		OutResponse.CapabilityNotes.Add(TEXT("Re-read after write failed — not claiming *_validated."));
		OutResponse.Metrics.InternalOperations = 4;
		OutResponse.Metrics.AssetsAffected = Attenuation ? 2 : 1;
		return true;
	}

	OutResponse.Status = TEXT("created_and_validated");
	OutResponse.Summary = FString::Printf(
		TEXT("Created and re-read SoundCue '%s' with %d wave(s)%s."),
		*Request.TargetAssetPath,
		Inspection.SoundWavePaths.Num(),
		Attenuation ? TEXT(" and attenuation") : TEXT(""));
	OutResponse.Revision = Inspection.ContentHash;
	OutResponse.Metrics.InternalOperations = 5;
	OutResponse.Metrics.AssetsAffected = Attenuation ? 2 : 1;
	OutResponse.ExtraFields = MakeShared<FJsonObject>();
	OutResponse.ExtraFields->SetStringField(TEXT("content_hash"), Inspection.ContentHash);
	if (Attenuation)
	{
		OutResponse.ExtraFields->SetStringField(TEXT("attenuation_path"), Attenuation->GetPathName());
		FUeremcpAssetRef AttRef;
		AttRef.AssetPath = Attenuation->GetOutermost()->GetName();
		AttRef.AssetClass = TEXT("SoundAttenuation");
		AttRef.Role = TEXT("attenuation");
		OutResponse.CreatedAssets.Add(AttRef);
	}
	FUeremcpAssetRef CueRef;
	CueRef.AssetPath = Request.TargetAssetPath;
	CueRef.AssetClass = TEXT("SoundCue");
	CueRef.Revision = Inspection.ContentHash;
	CueRef.Role = TEXT("primary");
	OutResponse.CreatedAssets.Add(CueRef);
	return true;
}
