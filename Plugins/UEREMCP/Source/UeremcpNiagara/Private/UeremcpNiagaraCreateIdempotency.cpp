// UEREMCP — ADR-0006 create idempotency / revision helpers for Niagara (WS-07).

#include "UeremcpNiagaraCreateIdempotency.h"

#include "UeremcpContentHash.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraProbeAssets.h"
#include "UeremcpNiagaraRoleNames.h"

#include "Dom/JsonObject.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraParameterStore.h"
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPath.h"

namespace UeremcpNiagaraCreateIdempotency
{
	UNiagaraSystem* TryLoadSystem(const FString& AssetPath)
	{
		const FString PackageFolder = UeremcpNiagaraPaths::PackageFolderFromAssetPath(AssetPath);
		const FString AssetName = UeremcpNiagaraPaths::AssetNameFromAssetPath(AssetPath);
		const FSoftObjectPath ObjectPath(
			FString::Printf(TEXT("%s/%s.%s"), *PackageFolder, *AssetName, *AssetName));
		return Cast<UNiagaraSystem>(ObjectPath.TryLoad());
	}

	TSharedPtr<FJsonObject> BuildFingerprint(UNiagaraSystem* System)
	{
		TSharedPtr<FJsonObject> Fingerprint = MakeShared<FJsonObject>();
		if (!System)
		{
			return Fingerprint;
		}

		TArray<FString> EmitterNames;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			EmitterNames.Add(Handle.GetName().ToString());
		}
		EmitterNames.Sort();

		TArray<TSharedPtr<FJsonValue>> EmitterValues;
		for (const FString& Name : EmitterNames)
		{
			EmitterValues.Add(MakeShared<FJsonValueString>(Name));
		}
		Fingerprint->SetArrayField(TEXT("emitters"), EmitterValues);

		TArray<FString> UserParamNames;
		// [VERIFIED: NiagaraParameterStore.h:182 ReadParameterVariables → FNiagaraVariableWithOffset]
		for (const FNiagaraVariableWithOffset& Variable :
			System->GetExposedParameters().ReadParameterVariables())
		{
			UserParamNames.Add(Variable.GetName().ToString());
		}
		UserParamNames.Sort();

		TArray<TSharedPtr<FJsonValue>> UserParamValues;
		for (const FString& Name : UserParamNames)
		{
			UserParamValues.Add(MakeShared<FJsonValueString>(Name));
		}
		Fingerprint->SetArrayField(TEXT("user_parameters"), UserParamValues);
		Fingerprint->SetStringField(TEXT("asset_class"), TEXT("NiagaraSystem"));
		return Fingerprint;
	}

	FString CreatedAssetPathFromRequest(const FString& TargetAssetPath, const FString& SpecName)
	{
		const FString PackageFolder = UeremcpNiagaraPaths::PackageFolderFromAssetPath(TargetAssetPath);
		const FString AssetName = SpecName.IsEmpty()
			? UeremcpNiagaraPaths::AssetNameFromAssetPath(TargetAssetPath)
			: SpecName;
		return FString::Printf(TEXT("%s/%s"), *PackageFolder, *AssetName);
	}

	bool TryComputeAssetRevision(const FString& AssetPath, FString& OutRevision, FString& OutError)
	{
		OutRevision.Reset();
		OutError.Reset();
		UNiagaraSystem* System = TryLoadSystem(AssetPath);
		if (!System)
		{
			OutError = FString::Printf(TEXT("Failed to load Niagara system '%s' for revision."), *AssetPath);
			return false;
		}

		const TSharedPtr<FJsonObject> Fingerprint = BuildFingerprint(System);
		OutRevision = FUeremcpContentHash::HashJsonObject(Fingerprint, &OutError);
		return !OutRevision.IsEmpty();
	}

	bool ExistingSatisfiesSpec(const FString& AssetPath, const FUeremcpNiagaraCreateSpec& Spec)
	{
		UNiagaraSystem* System = TryLoadSystem(AssetPath);
		if (!System)
		{
			return false;
		}

		TSet<FString> EmitterNames;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			EmitterNames.Add(Handle.GetName().ToString());
		}

		if (Spec.Emitters.Num() > 0)
		{
			for (const FUeremcpNiagaraEmitterPlan& Plan : Spec.Emitters)
			{
				if (!EmitterNames.Contains(Plan.Name))
				{
					return false;
				}
			}
			return true;
		}

		if (Spec.ComponentRoles.Num() == 0)
		{
			return EmitterNames.Num() > 0;
		}

		for (const FString& Role : Spec.ComponentRoles)
		{
			const FString EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Role);
			if (!EmitterNames.Contains(EmitterName))
			{
				return false;
			}
		}
		return true;
	}

	bool ShouldBypassRevisionConflict(const FString& OnRevisionConflict)
	{
		return OnRevisionConflict.Equals(TEXT("replace"), ESearchCase::CaseSensitive)
			|| OnRevisionConflict.Equals(TEXT("force"), ESearchCase::CaseSensitive);
	}
}
