// UEREMCP — probe-scoped asset lifecycle helpers (WS-07).

#include "UeremcpNiagaraProbeAssets.h"

#include "UeremcpNiagaraPaths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraSystem.h"
#include "ObjectTools.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	UObject* LoadAssetAtPath(const FString& AssetPath)
	{
		const FString PackagePath = UeremcpNiagaraPaths::PackageFolderFromAssetPath(AssetPath);
		const FString AssetName = UeremcpNiagaraPaths::AssetNameFromAssetPath(AssetPath);
		const FSoftObjectPath ObjectPath(
			FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName));
		return ObjectPath.TryLoad();
	}
}

bool UeremcpNiagaraProbeAssets::IsReplaceMode(const FString& Mode)
{
	return Mode.Equals(TEXT("replace"), ESearchCase::IgnoreCase);
}

bool UeremcpNiagaraProbeAssets::AssetExistsAtPath(const FString& AssetPath)
{
	FAssetRegistryModule& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const FString PackagePath = UeremcpNiagaraPaths::PackageFolderFromAssetPath(AssetPath);
	const FString AssetName = UeremcpNiagaraPaths::AssetNameFromAssetPath(AssetPath);
	const FSoftObjectPath ObjectPath(
		FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName));
	return AssetRegistry.Get().GetAssetByObjectPath(ObjectPath).IsValid();
}

bool UeremcpNiagaraProbeAssets::DeleteProbeAssetAtPath(const FString& AssetPath, FString& OutError)
{
	OutError.Reset();

	if (!UeremcpNiagaraPaths::IsAllowedProbePath(AssetPath))
	{
		OutError = FString::Printf(
			TEXT("Refusing to delete asset outside probe root %s (got '%s')."),
			UeremcpNiagaraPaths::TestsContentRoot,
			*AssetPath);
		return false;
	}

	UObject* Existing = LoadAssetAtPath(AssetPath);
	if (!Existing)
	{
		return true;
	}

	TArray<UObject*> ToDelete;
	ToDelete.Add(Existing);

	const int32 DeletedCount = ObjectTools::DeleteObjectsUnchecked(ToDelete);
	if (DeletedCount != 1)
	{
		OutError = FString::Printf(
			TEXT("DeleteObjectsUnchecked removed %d object(s) at '%s'; expected 1."),
			DeletedCount,
			*AssetPath);
		return false;
	}

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return true;
}
