// UEREMCP — procedural texture orchestration (WS-08).

#include "UeremcpProceduralTextureService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"
#include "UeremcpMaterialAssetLoad.h"
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
		if (Kind == TEXT("flipbook_atlas"))
		{
			OutParams.bSRGB = true;
			OutParams.bUseAlpha = false;
			OutParams.CompressionSettings = TC_Default;
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

	static bool ReadTextureDimensions(const UTexture2D* Texture, int32& OutWidth, int32& OutHeight)
	{
		if (!Texture)
		{
			return false;
		}

		if (Texture->Source.IsValid())
		{
			OutWidth = Texture->Source.GetSizeX();
			OutHeight = Texture->Source.GetSizeY();
			if (OutWidth > 0 && OutHeight > 0)
			{
				return true;
			}
		}

		OutWidth = Texture->GetSizeX();
		OutHeight = Texture->GetSizeY();
		return OutWidth > 0 && OutHeight > 0;
	}
}

bool UeremcpProceduralTextureService::IsSupportedGenerateKind(const FString& Kind)
{
	return Kind == TEXT("noise") ||
		Kind == TEXT("gradient") ||
		Kind == TEXT("voronoi") ||
		Kind == TEXT("ring_mask") ||
		Kind == TEXT("flow_map") ||
		Kind == TEXT("flipbook_atlas") ||
		Kind == TEXT("flipbook_import");
}

bool UeremcpProceduralTextureService::IsImplementedGenerateKind(const FString& Kind)
{
	return IsSupportedGenerateKind(Kind) && Kind != TEXT("flipbook_import");
}

bool UeremcpProceduralTextureService::ParseGenerateSpec(
	const TSharedPtr<FJsonObject>& GenerateObject,
	FString& OutKind,
	int32& OutWidth,
	int32& OutHeight,
	int32& OutSeed,
	int32& OutFlipbookColumns,
	int32& OutFlipbookRows,
	int32& OutFlipbookFrameCount,
	FString& OutSourceFilePath)
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
	OutFlipbookColumns = 0;
	OutFlipbookRows = 0;
	OutFlipbookFrameCount = 0;
	OutSourceFilePath.Reset();

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

	const TSharedPtr<FJsonObject>* FlipbookObject = nullptr;
	if (GenerateObject->TryGetObjectField(TEXT("flipbook"), FlipbookObject) && FlipbookObject && FlipbookObject->IsValid())
	{
		double Columns = 0.0;
		double Rows = 0.0;
		if ((*FlipbookObject)->TryGetNumberField(TEXT("columns"), Columns))
		{
			OutFlipbookColumns = static_cast<int32>(Columns);
		}
		if ((*FlipbookObject)->TryGetNumberField(TEXT("rows"), Rows))
		{
			OutFlipbookRows = static_cast<int32>(Rows);
		}
		double FrameCount = 0.0;
		if ((*FlipbookObject)->TryGetNumberField(TEXT("frame_count"), FrameCount))
		{
			OutFlipbookFrameCount = static_cast<int32>(FrameCount);
		}
	}

	if (OutKind == TEXT("flipbook_atlas"))
	{
		if (OutFlipbookColumns <= 0 || OutFlipbookRows <= 0)
		{
			return false;
		}
		if (OutFlipbookFrameCount <= 0)
		{
			OutFlipbookFrameCount = OutFlipbookColumns * OutFlipbookRows;
		}
	}

	if (OutKind == TEXT("flipbook_import"))
	{
		const TSharedPtr<FJsonObject>* SourceObject = nullptr;
		if (!GenerateObject->TryGetObjectField(TEXT("source"), SourceObject) || !SourceObject || !SourceObject->IsValid())
		{
			return false;
		}
		if (!(*SourceObject)->TryGetStringField(TEXT("file_path"), OutSourceFilePath) || OutSourceFilePath.IsEmpty())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* ImportFlipbookObject = nullptr;
		if (!(*SourceObject)->TryGetObjectField(TEXT("flipbook"), ImportFlipbookObject) ||
			!ImportFlipbookObject ||
			!ImportFlipbookObject->IsValid())
		{
			return false;
		}

		double Columns = 0.0;
		double Rows = 0.0;
		if (!(*ImportFlipbookObject)->TryGetNumberField(TEXT("columns"), Columns) ||
			!(*ImportFlipbookObject)->TryGetNumberField(TEXT("rows"), Rows))
		{
			return false;
		}
		OutFlipbookColumns = static_cast<int32>(Columns);
		OutFlipbookRows = static_cast<int32>(Rows);
		if (OutFlipbookColumns <= 0 || OutFlipbookRows <= 0)
		{
			return false;
		}

		double FrameCount = 0.0;
		if ((*ImportFlipbookObject)->TryGetNumberField(TEXT("frame_count"), FrameCount))
		{
			OutFlipbookFrameCount = static_cast<int32>(FrameCount);
		}
		if (OutFlipbookFrameCount <= 0)
		{
			OutFlipbookFrameCount = OutFlipbookColumns * OutFlipbookRows;
		}
	}

	return true;
}

FUeremcpProceduralTextureResult UeremcpProceduralTextureService::Execute(
	const FUeremcpProceduralTextureRequest& Request)
{
	FUeremcpProceduralTextureResult Result;
	Result.CapabilityNotes = {
		TEXT("procedural_texture_v1: CPU pixel fill via FImageUtils::CreateTexture2D — not Epic MaterialTools/RT draw."),
		TEXT("flipbook_atlas: CPU grid assembly of procedural per-frame cells — not external sheet import."),
		TEXT("flipbook_import: Phase A scaffold — FImageUtils::ImportBufferAsTexture2D not invoked [VERIFIED: ImageUtils.h:448-449]."),
	};

	if (!GEditor)
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = TEXT("create_procedural_texture requires the Unreal Editor.");
		return Result;
	}

	if (!UeremcpMaterialPaths::IsUnderAllowedScratchRoot(Request.TargetAssetPath))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT(
			"Procedural textures may only be written under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/.");
		return Result;
	}

	if (!IsSupportedGenerateKind(Request.GenerateKind))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(TEXT("Unsupported generate kind '%s'."), *Request.GenerateKind);
		return Result;
	}

	if (!IsImplementedGenerateKind(Request.GenerateKind))
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = FString::Printf(
			TEXT("generate kind '%s' is recognized but not implemented (source '%s', grid %dx%d, %d frames)."),
			*Request.GenerateKind,
			*Request.SourceFilePath,
			Request.FlipbookColumns,
			Request.FlipbookRows,
			Request.FlipbookFrameCount);
		Result.InterpretationNotes.Add(TEXT(
			"flipbook_import Phase A scaffold: specification parsed; "
			"FImageUtils::ImportBufferAsTexture2D not invoked [VERIFIED: ImageUtils.h:448-449]."));
		return Result;
	}

	UEditorAssetSubsystem* AssetSubsystem = GetEditorAssetSubsystem();
	if (!AssetSubsystem)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("EditorAssetSubsystem unavailable.");
		return Result;
	}

	if (UTexture2D* Existing = UeremcpMaterialAssetLoad::TryLoadTexture(Request.TargetAssetPath))
	{
		Result.bSuccess = true;
		Result.bReused = true;
		Result.Status = TEXT("no_change_required");
		Result.PrimaryAsset = Request.TargetAssetPath;
		if (!ReadTextureDimensions(Existing, Result.VerifiedWidth, Result.VerifiedHeight))
		{
			Result.VerifiedWidth = Request.Width;
			Result.VerifiedHeight = Request.Height;
		}
		Result.Summary = FString::Printf(
			TEXT("Reused existing texture '%s' (%dx%d)."),
			*Request.TargetAssetPath,
			Result.VerifiedWidth,
			Result.VerifiedHeight);
		Result.InterpretationNotes.Add(TEXT("Texture already existed; generation skipped (idempotent reuse)."));
		FUeremcpAssetRef Reused;
		Reused.AssetPath = Request.TargetAssetPath;
		Reused.AssetClass = TEXT("Texture2D");
		Reused.Role = Request.GenerateKind;
		Result.ReusedAssets.Add(Reused);
		return Result;
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
	const bool bPixelsOk =
		Request.GenerateKind == TEXT("flipbook_atlas")
			? UeremcpProceduralTextureGenerator::GenerateFlipbookAtlasPixels(
				Request.Width,
				Request.Height,
				Request.FlipbookColumns,
				Request.FlipbookRows,
				Request.FlipbookFrameCount,
				Seed,
				Pixels,
				PixelError)
			: UeremcpProceduralTextureGenerator::GeneratePixels(
				Request.GenerateKind,
				Request.Width,
				Request.Height,
				Seed,
				Pixels,
				PixelError);
	if (!bPixelsOk)
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

	UTexture2D* Reloaded = UeremcpMaterialAssetLoad::TryLoadTexture(Request.TargetAssetPath);
	const UTexture2D* DimensionProbe = Reloaded ? Reloaded : Texture;
	if (!DimensionProbe)
	{
		Result.Status = TEXT("partially_completed");
		Result.Summary = TEXT("Texture saved but reload verification failed.");
		Result.PrimaryAsset = Request.TargetAssetPath;
		Result.bSuccess = true;
		Result.bCreated = true;
		Result.CapabilityNotes.Add(
			TEXT("validate: texture reload unavailable — cannot claim created_and_validated."));
		return Result;
	}

	if (!ReadTextureDimensions(DimensionProbe, Result.VerifiedWidth, Result.VerifiedHeight))
	{
		Result.bSuccess = true;
		Result.bCreated = true;
		Result.Status = TEXT("partially_completed");
		Result.PrimaryAsset = Request.TargetAssetPath;
		Result.Summary = FString::Printf(
			TEXT("Created procedural texture '%s' (%s, requested %dx%d) but dimension re-read was unavailable (NullRHI or unloaded source)."),
			*Request.TargetAssetPath,
			*Request.GenerateKind,
			Request.Width,
			Request.Height);
		Result.CapabilityNotes.Add(
			TEXT("validate: texture dimension re-read unavailable under NullRHI — cannot claim created_and_validated."));
		Result.InterpretationNotes.Add(
			TEXT("options.validate=true but post-save dimension proof failed; status capped at partially_completed."));

		FUeremcpAssetRef Created;
		Created.AssetPath = Request.TargetAssetPath;
		Created.AssetClass = TEXT("Texture2D");
		Created.Role = Request.GenerateKind;
		Result.CreatedAssets.Add(Created);
		return Result;
	}

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
	int32 FlipbookColumns = 0;
	int32 FlipbookRows = 0;
	int32 FlipbookFrameCount = 0;
	FString SourceFilePath;
	if (!ParseGenerateSpec(
		Request.Specification, Kind, Width, Height, Seed, FlipbookColumns, FlipbookRows, FlipbookFrameCount, SourceFilePath))
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
	TextureRequest.FlipbookColumns = FlipbookColumns;
	TextureRequest.FlipbookRows = FlipbookRows;
	TextureRequest.FlipbookFrameCount = FlipbookFrameCount;
	TextureRequest.SourceFilePath = SourceFilePath;
	TextureRequest.bDryRun = Request.bDryRun;
	TextureRequest.bSave = Request.bSave;
	TextureRequest.bValidate = Request.bValidate;

	return Execute(TextureRequest);
}
