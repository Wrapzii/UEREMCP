// UEREMCP — editor asset load helpers (WS-08).

#include "UeremcpMaterialAssetLoad.h"

#include "Editor.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialPaths.h"
#include "UObject/UObjectGlobals.h"

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

UMaterialInstanceConstant* UeremcpMaterialAssetLoad::TryLoadMaterialInstance(const FString& PackagePath)
{
	if (UMaterialInstanceConstant* Registered = Cast<UMaterialInstanceConstant>(TryLoadRegisteredAsset(PackagePath)))
	{
		return Registered;
	}

	return FindInProcessObject<UMaterialInstanceConstant>(PackagePath);
}

UTexture* UeremcpMaterialAssetLoad::TryLoadTexture(const FString& PackagePath)
{
	if (UTexture* Registered = Cast<UTexture>(TryLoadRegisteredAsset(PackagePath)))
	{
		return Registered;
	}

	return FindInProcessObject<UTexture>(PackagePath);
}
