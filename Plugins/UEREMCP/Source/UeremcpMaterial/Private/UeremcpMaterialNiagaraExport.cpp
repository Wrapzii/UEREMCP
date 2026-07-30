// UEREMCP — WS-07 Niagara material binding export (WS-08).

#include "UeremcpMaterialNiagaraExport.h"

#include "Editor.h"
#include "Materials/MaterialInterface.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	static UEditorAssetSubsystem* GetEditorAssetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	}

	static TSharedPtr<FJsonObject> CloneSpecWithDefaultPurpose(
		const TSharedPtr<FJsonObject>& CreateSpec,
		const FString& Role)
	{
		if (!CreateSpec.IsValid())
		{
			return nullptr;
		}
		if (CreateSpec->HasField(TEXT("purpose")))
		{
			return CreateSpec;
		}

		FString Purpose;
		if (!UeremcpMaterialNiagaraExport::ResolvePurposeForNiagaraRole(Role, Purpose))
		{
			return CreateSpec;
		}

		TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>(*CreateSpec);
		Merged->SetStringField(TEXT("purpose"), Purpose);
		return Merged;
	}
}

FString UeremcpMaterialNiagaraExport::SanitizeAssetToken(const FString& Token)
{
	FString Out;
	Out.Reserve(Token.Len());
	for (const TCHAR Character : Token)
	{
		if (FChar::IsAlnum(Character) || Character == TEXT('_'))
		{
			Out.AppendChar(Character);
		}
		else
		{
			Out.AppendChar(TEXT('_'));
		}
	}
	if (Out.IsEmpty())
	{
		return TEXT("Unnamed");
	}
	return Out;
}

FString UeremcpMaterialNiagaraExport::ResolveMaterialInstancePath(
	const FString& NiagaraAssetName,
	const FString& Role,
	const FString& ScratchContentRoot)
{
	const FString SafeName = SanitizeAssetToken(NiagaraAssetName);
	const FString SafeRole = SanitizeAssetToken(Role);
	const FString AssetName = FString::Printf(TEXT("MI_%s_%s"), *SafeName, *SafeRole);
	return UeremcpMaterialPaths::JoinPackagePath(
		UeremcpMaterialPaths::MaterialsFolderForContentRoot(ScratchContentRoot),
		AssetName);
}

FString UeremcpMaterialNiagaraExport::ResolveMaterialInstancePathForNiagaraSystem(
	const FString& NiagaraSystemPackagePath,
	const FString& Role)
{
	FString Folder;
	FString SystemName;
	if (!UeremcpMaterialPaths::SplitPackagePath(NiagaraSystemPackagePath, Folder, SystemName))
	{
		return FString();
	}

	const FString ScratchContentRoot =
		UeremcpMaterialPaths::ResolveScratchContentRoot(NiagaraSystemPackagePath);
	if (ScratchContentRoot.IsEmpty())
	{
		return FString();
	}

	return ResolveMaterialInstancePath(SystemName, Role, ScratchContentRoot);
}

bool UeremcpMaterialNiagaraExport::ResolvePurposeForNiagaraRole(const FString& Role, FString& OutPurpose)
{
	const FString Key = Role.ToLower();
	if (Key == TEXT("core") || Key == TEXT("core_material"))
	{
		OutPurpose = TEXT("elemental_projectile_core");
		return true;
	}
	if (Key == TEXT("ribbon_trail") || Key == TEXT("trail") || Key == TEXT("trail_material"))
	{
		OutPurpose = TEXT("elemental_projectile_trail");
		return true;
	}
	return false;
}

FUeremcpRequest UeremcpMaterialNiagaraExport::BuildCreateVfxMaterialRequest(
	const FString& TargetAssetPath,
	const TSharedPtr<FJsonObject>& CreateSpec,
	bool bCompile,
	bool bValidate,
	bool bSave)
{
	FUeremcpRequest Request;
	Request.Action = TEXT("create_vfx_material");
	Request.TargetAssetPath = TargetAssetPath;
	Request.Specification = CreateSpec;
	Request.bCompile = bCompile;
	Request.bValidate = bValidate;
	Request.bSave = bSave;
	return Request;
}

bool UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface(
	const FString& PackagePath,
	FString& OutError)
{
	if (PackagePath.IsEmpty())
	{
		OutError = TEXT("PrimaryAsset path is empty.");
		return false;
	}

	const FSoftObjectPath SoftPath(PackagePath);
	if (!SoftPath.IsValid())
	{
		OutError = FString::Printf(TEXT("PrimaryAsset '%s' is not FSoftObjectPath-valid."), *PackagePath);
		return false;
	}

	UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
	if (!AssetSubsystem)
	{
		OutError = TEXT("EditorAssetSubsystem unavailable for PrimaryAsset verification.");
		return false;
	}

	UObject* Loaded = UeremcpMaterialAssetLoad::TryLoadRegisteredAsset(PackagePath);
	UMaterialInterface* Material = Cast<UMaterialInterface>(Loaded);
	if (!Material)
	{
		OutError = FString::Printf(
			TEXT("PrimaryAsset '%s' did not load as UMaterialInterface."),
			*PackagePath);
		return false;
	}

	return true;
}

FUeremcpMaterialCreateResult UeremcpMaterialNiagaraExport::ExecuteCreateVfxMaterialForNiagaraRole(
	const FString& NiagaraAssetName,
	const FString& Role,
	const TSharedPtr<FJsonObject>& CreateSpec,
	bool bCompile,
	bool bValidate,
	bool bSave)
{
	const FString TargetPath = ResolveMaterialInstancePath(NiagaraAssetName, Role);
	const TSharedPtr<FJsonObject> EffectiveSpec = CloneSpecWithDefaultPurpose(CreateSpec, Role);
	const FUeremcpRequest Request =
		BuildCreateVfxMaterialRequest(TargetPath, EffectiveSpec, bCompile, bValidate, bSave);
	return UeremcpMaterialService::ExecuteCreateVfxMaterial(Request);
}
