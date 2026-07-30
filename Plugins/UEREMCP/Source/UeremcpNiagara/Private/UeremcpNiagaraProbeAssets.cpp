// UEREMCP — probe-scoped asset lifecycle helpers (WS-07).

#include "UeremcpNiagaraProbeAssets.h"

#include "UeremcpNiagaraPaths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraDataInterfaceMeshRendererInfo.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "ObjectTools.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

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

	bool RendererBelongsToSystem(UNiagaraMeshRendererProperties* Renderer, UNiagaraSystem* System)
	{
		if (!Renderer || !System)
		{
			return false;
		}

		for (UObject* Outer = Renderer; Outer; Outer = Outer->GetOuter())
		{
			if (Outer == System)
			{
				return true;
			}
		}
		return false;
	}

	void ReleaseMeshRendererInfoReferences(UNiagaraSystem* System)
	{
#if WITH_EDITOR
		if (!System)
		{
			return;
		}

		for (TObjectIterator<UNiagaraDataInterfaceMeshRendererInfo> It; It; ++It)
		{
			UNiagaraDataInterfaceMeshRendererInfo* DI = *It;
			if (!DI || !DI->GetMeshRenderer())
			{
				continue;
			}

			if (RendererBelongsToSystem(DI->GetMeshRenderer(), System))
			{
				// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraDataInterfaceMeshRendererInfo.h:38]
				DI->OnMeshRendererChanged(nullptr);
				// OnMeshRendererChanged(nullptr) removes delegates but leaves MeshRenderer set;
				// clear the property so ForceDeleteObjects does not see an external referencer.
				// [VERIFIED: NiagaraDataInterfaceMeshRendererInfo.h:72, NiagaraDataInterfaceMeshRendererInfo.cpp:122-137]
				if (FObjectProperty* MeshRendererProp = FindFProperty<FObjectProperty>(
					UNiagaraDataInterfaceMeshRendererInfo::StaticClass(),
					TEXT("MeshRenderer")))
				{
					MeshRendererProp->SetObjectPropertyValue_InContainer(DI, nullptr);
				}
				DI->ConditionalBeginDestroy();
			}
		}
#endif
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

void UeremcpNiagaraProbeAssets::ReleaseExternalReferences(UNiagaraSystem* System)
{
	if (!System)
	{
		return;
	}

	// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h:457]
	System->KillAllActiveCompilations();
	// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h:461]
	System->InvalidateActiveCompiles();
	ReleaseMeshRendererInfoReferences(System);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
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

	if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Existing))
	{
		ReleaseExternalReferences(System);
	}

	TArray<UObject*> ToDelete;
	ToDelete.Add(Existing);
	Existing = nullptr;

	const int32 DeletedCount = ObjectTools::DeleteObjectsUnchecked(ToDelete);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	if (DeletedCount != 1)
	{
		OutError = FString::Printf(
			TEXT("DeleteObjectsUnchecked removed %d object(s) at '%s'; expected 1."),
			DeletedCount,
			*AssetPath);
		return false;
	}

	return true;
}
