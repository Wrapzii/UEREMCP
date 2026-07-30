// UEREMCP — editor asset load helpers that avoid LogError on expected misses (WS-08).
//
// EditorAssetSubsystem::LoadAsset logs Error when an asset is absent from the registry.
// Automation treats those LogError lines as test failures even when create_vfx_material
// honestly returns partially_completed under NullRHI. Gate loads with DoesAssetExist first.

#pragma once

#include "CoreMinimal.h"

class UEditorAssetSubsystem;
class UMaterial;
class UMaterialInstanceConstant;
class UObject;
class UTexture2D;

namespace UeremcpMaterialAssetLoad
{
	/** Load only when the asset registry reports the package exists (no LogError on miss). */
	UEREMCPMATERIAL_API UObject* TryLoadRegisteredAsset(const FString& PackagePath);

	/** Registry-only material load for idempotent reuse (ignores stale in-process objects after delete). */
	UEREMCPMATERIAL_API UMaterial* TryLoadRegisteredMaterial(const FString& PackagePath);

	UEREMCPMATERIAL_API UMaterial* ResolveMaterial(const FString& PackagePath, UMaterial* Preferred = nullptr);

	UEREMCPMATERIAL_API UMaterialInstanceConstant* TryLoadMaterialInstance(const FString& PackagePath);

	UEREMCPMATERIAL_API UTexture2D* TryLoadTexture(const FString& PackagePath);
}
