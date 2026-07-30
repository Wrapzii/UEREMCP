// UEREMCP — editor asset load helpers (WS-08).

#include "UeremcpMaterialAssetLoad.h"

#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UeremcpMaterialPaths.h"
#include "PackageTools.h"

namespace
{
	static UEditorAssetSubsystem* GetEditorAssetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	}

	static FString ObjectPathFromPackage(const FString& PackagePath)
	{
		FString FolderPath;
		FString AssetName;
		if (UeremcpMaterialPaths::SplitPackagePath(PackagePath, FolderPath, AssetName))
		{
			return FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
		}
		return PackagePath;
	}

	template<typename TObjectType>
	static TObjectType* FindInProcessObject(const FString& PackagePath)
	{
		const FString ObjectPath = ObjectPathFromPackage(PackagePath);
		if (TObjectType* Found = FindObject<TObjectType>(nullptr, *ObjectPath))
		{
			return Found;
		}
		return Cast<TObjectType>(StaticLoadObject(TObjectType::StaticClass(), nullptr, *ObjectPath));
	}
}

UObject* UeremcpMaterialAssetLoad::TryLoadRegisteredAsset(const FString& PackagePath)
{
	UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
	if (!AssetSubsystem || PackagePath.IsEmpty())
	{
		return nullptr;
	}

	if (!AssetSubsystem->DoesAssetExist(PackagePath))
	{
		return nullptr;
	}

	return AssetSubsystem->LoadAsset(PackagePath);
}

UMaterial* UeremcpMaterialAssetLoad::TryLoadRegisteredMaterial(const FString& PackagePath)
{
	return Cast<UMaterial>(TryLoadRegisteredAsset(PackagePath));
}

UMaterial* UeremcpMaterialAssetLoad::ResolveMaterial(const FString& PackagePath, UMaterial* Preferred)
{
	if (Preferred)
	{
		return Preferred;
	}

	if (UMaterial* Registered = Cast<UMaterial>(TryLoadRegisteredAsset(PackagePath)))
	{
		return Registered;
	}

	return FindInProcessObject<UMaterial>(PackagePath);
}

UMaterialInstanceConstant* UeremcpMaterialAssetLoad::TryLoadRegisteredMaterialInstance(const FString& PackagePath)
{
	return Cast<UMaterialInstanceConstant>(TryLoadRegisteredAsset(PackagePath));
}

UMaterialInstanceConstant* UeremcpMaterialAssetLoad::TryLoadMaterialInstance(const FString& PackagePath)
{
	if (UMaterialInstanceConstant* Registered = Cast<UMaterialInstanceConstant>(TryLoadRegisteredAsset(PackagePath)))
	{
		return Registered;
	}

	return FindInProcessObject<UMaterialInstanceConstant>(PackagePath);
}

UTexture2D* UeremcpMaterialAssetLoad::TryLoadTexture(const FString& PackagePath)
{
	if (UTexture2D* Registered = Cast<UTexture2D>(TryLoadRegisteredAsset(PackagePath)))
	{
		return Registered;
	}

	return FindInProcessObject<UTexture2D>(PackagePath);
}

void UeremcpMaterialAssetLoad::ReleasePackageForCreate(
	const FString& PackagePath,
	UEditorAssetSubsystem* AssetSubsystem)
{
	if (PackagePath.IsEmpty())
	{
		return;
	}

	if (!AssetSubsystem)
	{
		AssetSubsystem = GetEditorAssetSubsystem();
	}

	if (AssetSubsystem && AssetSubsystem->DoesAssetExist(PackagePath))
	{
		AssetSubsystem->DeleteAsset(PackagePath);
	}

	if (UPackage* ExistingPackage = FindPackage(nullptr, *PackagePath))
	{
		TArray<UPackage*> PackagesToUnload;
		PackagesToUnload.Add(ExistingPackage);
		UPackageTools::UnloadPackages(PackagesToUnload);
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackagePath,
		FPackageName::GetAssetPackageExtension());
	if (FPaths::FileExists(PackageFilename))
	{
		IFileManager::Get().Delete(*PackageFilename);
	}
}
