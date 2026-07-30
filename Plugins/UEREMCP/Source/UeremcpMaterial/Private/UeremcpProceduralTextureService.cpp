// UEREMCP — procedural texture orchestration (WS-08).

#include "UeremcpProceduralTextureService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"
#include "UeremcpMaterialPaths.h"
#include "UeremcpProceduralTextureGenerator.h"

namespace
{
	static UEditorAssetSubsystem* GetEditorAssetSubsystem()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	}

	static void ConfigureCreateParams(const FString& Kind, FCreateTexture2DParameters& OutParams)
	{
		OutParams.bDeferCompression = false;
		OutParams.bVirtualTexture = false;
		OutParams.MipGenSettings = TMGS_NoMipmaps;

		if (Kind == TEXT("flow_map"))
		{
			OutParams.bSRGB = false;
			OutParams.bUseAlpha = false;
			OutParams.CompressionSettings = TC_VectorDisplacementmap;
			return;
		}
		if (Kind == TEXT("ring_mask"))
		{
			OutParams.bSRGB = false;
			OutParams.bUseAlpha = true;
			OutParams.CompressionSettings = TC_Grayscale;
			return;
		}
		if (Kind == TEXT("noise") || Kind == TEXT("voronoi"))
		{
			OutParams.bSRGB = false;
			OutParams.bUseAlpha = false;
			OutParams.CompressionSettings = TC_Grayscale;
			return;
		}
		OutParams.bSRGB = true;
		OutParams.bUseAlpha = false;
		OutParams.CompressionSettings = TC_Default;
	}

	static int32 DefaultSeedForPath(const FString& Path, int32 SpecifiedSeed)
	{
		if (SpecifiedSeed != 0)
		{
			return SpecifiedSeed;
		}
		return static_cast<int32>(FCrc::StrCrc32(*Path) & 0x7FFFFFFF);
	}
}

bool UeremcpProceduralTextureService::IsSupportedGenerateKind(const FString& Kind)
{
	return Kind == TEXT("noise") ||
		Kind == TEXT("gradient") ||
		Kind == TEXT("voronoi") ||
		Kind == TEXT("ring_mask") ||
		Kind == TEXT("flow_map");
}

bool UeremcpProceduralTextureService::ParseGenerateSpec(
	const TSharedPtr<FJsonObject>& GenerateObject,
	FString& OutKind,
	int32& OutWidth,
	int32& OutHeight,
	int32& OutSeed)
{
	if (!GenerateObject.IsValid())
	{
		return false;
	}

	if (!GenerateObject->TryGetStringField(TEXT("generate"), OutKind) || OutKind.IsEmpty())
	{
		return false;
	}

	OutWidth = 512;
	OutHeight = 512;
	OutSeed = 0;

	const TArray<TSharedPtr<FJsonValue>>* Dimensions = nullptr;
	if (GenerateObject->TryGetArrayField(TEXT("dimensions"), Dimensions) && Dimensions && Dimensions->Num() >= 2)
	{
		OutWidth = static_cast<int32>((*Dimensions)[0]->AsNumber());
		OutHeight = static_cast<int32>((*Dimensions)[1]->AsNumber());
	}

	double SeedNumber = 0.0;
	if (GenerateObject->TryGetNumberField(TEXT("seed"), SeedNumber))
	{
		OutSeed = static_cast<int32>(SeedNumber);
	}

	return true;
}

FUeremcpProceduralTextureResult UeremcpProceduralTextureService::Execute(
	const FUeremcpProceduralTextureRequest& Request)
{
	FUeremcpProceduralTextureResult Result;
	Result.CapabilityNotes = {
		TEXT("procedural_texture_v1: CPU pixel fill via FImageUtils::CreateTexture2D — not Epic MaterialTools/RT draw."),
		TEXT("flipbook_subuv assembly is not implemented."),
	};

	if (!GEditor)
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = TEXT("create_procedural_texture requires the Unreal Editor.");
		return Result;
	}

	if (!UeremcpMaterialPaths::IsUnderTestsRoot(Request.TargetAssetPath))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("Procedural textures may only be written under /Game/__UeremcpTests/.");
		return Result;
	}

	if (!IsSupportedGenerateKind(Request.GenerateKind))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(TEXT("Unsupported generate kind '%s'."), *Request.GenerateKind);
		return Result;
	}

	UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
	if (!AssetSubsystem)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("EditorAssetSubsystem unavailable.");
		return Result;
	}

	if (AssetSubsystem->DoesAssetExist(Request.TargetAssetPath))
	{
		if (UTexture2D* Existing = Cast<UTexture2D>(AssetSubsystem->LoadAsset(Request.TargetAssetPath)))
		{
			Result.bSuccess = true;
			Result.bReused = true;
			Result.Status = TEXT("no_change_required");
			Result.PrimaryAsset = Request.TargetAssetPath;
			Result.VerifiedWidth = Existing->GetSizeX();
			Result.VerifiedHeight = Existing->GetSizeY();
			Result.Summary = FString::Printf(
				TEXT("Reused existing texture '%s' (%dx%d)."),
				*Request.TargetAssetPath,
				Result.VerifiedWidth,
				Result.VerifiedHeight);
			Result.InterpretationNotes.Add(TEXT("Texture already existed; generation skipped (idempotent reuse)."));
			return Result;
		}
	}

	if (Request.bDryRun)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("no_change_required");
		Result.PrimaryAsset = Request.TargetAssetPath;
		Result.Summary = FString::Printf(
			TEXT("dry_run: would generate '%s' (%s %dx%d)."),
			*Request.TargetAssetPath,
			*Request.GenerateKind,
			Request.Width,
			Request.Height);
		return Result;
	}

	TArray<FColor> Pixels;
	FString PixelError;
	const int32 Seed = DefaultSeedForPath(Request.TargetAssetPath, Request.Seed);
	if (!UeremcpProceduralTextureGenerator::GeneratePixels(
		Request.GenerateKind, Request.Width, Request.Height, Seed, Pixels, PixelError))
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = PixelError;
		return Result;
	}
	++Result.InternalOperations;

	FString FolderPath;
	FString AssetName;
	if (!UeremcpMaterialPaths::SplitPackagePath(Request.TargetAssetPath, FolderPath, AssetName))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(TEXT("Invalid target.asset_path '%s'."), *Request.TargetAssetPath);
		return Result;
	}

	UPackage* Package = CreatePackage(*Request.TargetAssetPath);
	if (!Package)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("CreatePackage failed.");
		return Result;
	}
	++Result.InternalOperations;

	FCreateTexture2DParameters CreateParams;
	ConfigureCreateParams(Request.GenerateKind, CreateParams);

	UTexture2D* Texture = FImageUtils::CreateTexture2D(
		Request.Width,
		Request.Height,
		Pixels,
		Package,
		AssetName,
		RF_Public | RF_Standalone,
		CreateParams);
	if (!Texture)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("FImageUtils::CreateTexture2D returned null.");
		return Result;
	}
	++Result.InternalOperations;

	FAssetRegistryModule::AssetCreated(Texture);
	Package->MarkPackageDirty();

	if (Request.bSave)
	{
		if (!AssetSubsystem->SaveAsset(Request.TargetAssetPath, false))
		{
			Result.Status = TEXT("partially_completed");
			Result.Summary = FString::Printf(
				TEXT("Texture created in memory but save failed for '%s'."),
				*Request.TargetAssetPath);
			Result.PrimaryAsset = Request.TargetAssetPath;
			return Result;
		}
		++Result.InternalOperations;
	}

	if (!Request.bValidate)
	{
		Result.bSuccess = true;
		Result.bCreated = true;
		Result.Status = TEXT("partially_completed");
		Result.PrimaryAsset = Request.TargetAssetPath;
		Result.Summary = FString::Printf(
			TEXT("Created procedural texture '%s' (%s, %dx%d, seed=%d); options.validate=false — dimension re-read skipped."),
			*Request.TargetAssetPath,
			*Request.GenerateKind,
			Request.Width,
			Request.Height,
			Seed);
		Result.InterpretationNotes.Add(
			TEXT("options.validate=false: envelope contract forbids *_validated status."));

		FUeremcpAssetRef Created;
		Created.AssetPath = Request.TargetAssetPath;
		Created.AssetClass = TEXT("Texture2D");
		Created.Role = Request.GenerateKind;
		Result.CreatedAssets.Add(Created);
		return Result;
	}

	UTexture2D* Reloaded = Cast<UTexture2D>(AssetSubsystem->LoadAsset(Request.TargetAssetPath));
	if (!Reloaded)
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = TEXT("Texture saved but reload verification failed.");
		Result.PrimaryAsset = Request.TargetAssetPath;
		return Result;
	}

	Result.VerifiedWidth = Reloaded->GetSizeX();
	Result.VerifiedHeight = Reloaded->GetSizeY();
	if (Result.VerifiedWidth != Request.Width || Result.VerifiedHeight != Request.Height)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = FString::Printf(
			TEXT("Post-save dimension mismatch: expected %dx%d, read %dx%d."),
			Request.Width,
			Request.Height,
			Result.VerifiedWidth,
			Result.VerifiedHeight);
		return Result;
	}

	Result.bSuccess = true;
	Result.bCreated = true;
	Result.Status = TEXT("created_and_validated");
	Result.PrimaryAsset = Request.TargetAssetPath;
	Result.Summary = FString::Printf(
		TEXT("Created procedural texture '%s' (%s, %dx%d, seed=%d)."),
		*Request.TargetAssetPath,
		*Request.GenerateKind,
		Result.VerifiedWidth,
		Result.VerifiedHeight,
		Seed);

	FUeremcpAssetRef Created;
	Created.AssetPath = Request.TargetAssetPath;
	Created.AssetClass = TEXT("Texture2D");
	Created.Role = Request.GenerateKind;
	Result.CreatedAssets.Add(Created);

	return Result;
}

FUeremcpProceduralTextureResult UeremcpProceduralTextureService::ExecuteFromEnvelope(
	const FUeremcpRequest& Request)
{
	FUeremcpProceduralTextureResult Result;

	if (Request.TargetAssetPath.IsEmpty())
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("create_procedural_texture requires target.asset_path.");
		return Result;
	}

	FString Kind;
	int32 Width = 512;
	int32 Height = 512;
	int32 Seed = 0;
	if (!ParseGenerateSpec(Request.Specification, Kind, Width, Height, Seed))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("create_procedural_texture requires specification.generate.");
		return Result;
	}

	FUeremcpProceduralTextureRequest TextureRequest;
	TextureRequest.TargetAssetPath = Request.TargetAssetPath;
	TextureRequest.GenerateKind = Kind;
	TextureRequest.Width = Width;
	TextureRequest.Height = Height;
	TextureRequest.Seed = Seed;
	TextureRequest.bDryRun = Request.bDryRun;
	TextureRequest.bSave = Request.bSave;
	TextureRequest.bValidate = Request.bValidate;

	return Execute(TextureRequest);
}
