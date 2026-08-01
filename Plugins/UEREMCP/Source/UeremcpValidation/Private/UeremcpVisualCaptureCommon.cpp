// UEREMCP — shared visual-capture primitives (WS-11).
#include "UeremcpVisualCaptureCommon.h"

#include "Components/SceneCaptureComponent2D.h"
#include "EngineUtils.h"
#include "GameFramework/Info.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"

namespace UeremcpVisualCapture
{
	FVector CameraOffset(const FString& Preset)
	{
		if (Preset == TEXT("front")) { return FVector(-900.0, 0.0, 300.0); }
		if (Preset == TEXT("side")) { return FVector(0.0, -950.0, 300.0); }
		if (Preset == TEXT("top")) { return FVector(-300.0, 0.0, 1050.0); }
		return FVector(-820.0, -560.0, 360.0); // three_quarter
	}

	bool ComputeWorldContentBounds(UWorld* World, FBox& OutBounds)
	{
		OutBounds.Init();
		if (!World)
		{
			return false;
		}

		bool bAny = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !IsValid(Actor) || Actor->IsTemporarilyHiddenInEditor())
			{
				continue;
			}
			if (Actor->IsA<AWorldSettings>() || Actor->IsA<AInfo>()
				|| Actor->IsA<ASceneCapture2D>())
			{
				continue;
			}

			FVector Origin;
			FVector Extent;
			Actor->GetActorBounds(false, Origin, Extent);
			if (Extent.GetAbsMax() < 1.0f)
			{
				continue;
			}
			OutBounds += FBox(Origin - Extent, Origin + Extent);
			bAny = true;
		}
		return bAny && OutBounds.IsValid;
	}

	FVector ScaledCameraOffset(const FString& Preset, const FBox& FocusBounds)
	{
		const FVector Base = CameraOffset(Preset);
		if (!FocusBounds.IsValid)
		{
			return Base;
		}
		const float Radius = FocusBounds.GetExtent().Size();
		const float Scale = FMath::Clamp(Radius / 400.0f, 0.35f, 8.0f);
		return Base * Scale;
	}

	FString MakeOutputDirectory(
		const FString& Domain,
		const FString& AssetLabel,
		const FString& RequestId)
	{
		const FString SafeDomain = FPaths::MakeValidFileName(
			Domain.IsEmpty() ? TEXT("Capture") : Domain);
		const FString SafeAsset = FPaths::MakeValidFileName(
			AssetLabel.IsEmpty() ? TEXT("unnamed") : AssetLabel);
		const FString SafeRun = FPaths::MakeValidFileName(
			RequestId.IsEmpty()
				? FGuid::NewGuid().ToString(EGuidFormats::Digits)
				: RequestId);
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("UEREMCP"),
			SafeDomain,
			SafeAsset,
			SafeRun);
	}

	bool IsPathUnderSavedUeremcp(const FString& AbsolutePath)
	{
		const FString Root = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEREMCP")));
		const FString Full = FPaths::ConvertRelativePathToFull(AbsolutePath);
		return Full.StartsWith(Root, ESearchCase::IgnoreCase);
	}

	bool ReadStats(UTextureRenderTarget2D* Target, FFrameStats& Out)
	{
		if (!Target)
		{
			return false;
		}
		FTextureRenderTargetResource* Resource =
			Target->GameThread_GetRenderTargetResource();
		if (!Resource)
		{
			return false;
		}

		TArray<FColor> Pixels;
		if (!Resource->ReadPixels(Pixels) || Pixels.IsEmpty())
		{
			return false;
		}

		double Total = 0.0;
		for (const FColor& Color : Pixels)
		{
			const double Luminance =
				0.2126 * Color.R + 0.7152 * Color.G + 0.0722 * Color.B;
			Total += Luminance;
			Out.LitPixels += Luminance > 40.0 ? 1 : 0;
			Out.Max = FMath::Max(Out.Max, Luminance);
		}
		Out.Mean = Total / static_cast<double>(Pixels.Num());
		return true;
	}

	bool IsNonEmptyPng(const FString& Path)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 8)
		{
			return false;
		}
		static constexpr uint8 PngSignature[8] =
			{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
		return FMemory::Memcmp(Bytes.GetData(), PngSignature, 8) == 0;
	}

	// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Kismet/KismetRenderingLibrary.h:38-48,141-144]
	bool CaptureExportAndVerify(
		UWorld* World,
		USceneCaptureComponent2D* Capture,
		UTextureRenderTarget2D* Target,
		const FString& Directory,
		const FString& FileName,
		FFrameStats& OutStats,
		int32& InternalOperations)
	{
		const FString Path = FPaths::Combine(Directory, FileName);
		if (!IsPathUnderSavedUeremcp(Path))
		{
			return false;
		}
		IFileManager::Get().Delete(*Path, false, true);
		UKismetRenderingLibrary::ClearRenderTarget2D(
			World, Target, FLinearColor(0.005f, 0.008f, 0.015f, 1.0f));
		++InternalOperations;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Capture->CaptureScene();
			++InternalOperations;
		}
		if (!ReadStats(Target, OutStats))
		{
			return false;
		}
		++InternalOperations;
		UKismetRenderingLibrary::ExportRenderTarget(
			World, Target, Directory, FileName);
		++InternalOperations;
		// ExportRenderTarget closes its writer synchronously, but the immediately
		// following read can still observe a not-yet-visible file on Windows.
		// Bound the verification retry so a valid exported PNG is not reported as
		// failed_validation while never accepting an absent or malformed file.
		for (int32 Attempt = 0; Attempt < 10; ++Attempt)
		{
			if (IsNonEmptyPng(Path))
			{
				return true;
			}
			FPlatformProcess::Sleep(0.01f);
		}
		return false;
	}

	void ApplyPinnedExposure(USceneCaptureComponent2D* Capture)
	{
		if (!Capture)
		{
			return;
		}
		FPostProcessSettings& Post = Capture->PostProcessSettings;
		Post.bOverride_AutoExposureMethod = true;
		Post.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		Post.bOverride_AutoExposureMinBrightness = true;
		Post.AutoExposureMinBrightness = 0.06f;
		Post.bOverride_AutoExposureMaxBrightness = true;
		Post.AutoExposureMaxBrightness = 0.06f;
		Post.bOverride_AutoExposureBias = true;
		Post.AutoExposureBias = 3.6f;
		Post.bOverride_AutoExposureSpeedUp = true;
		Post.AutoExposureSpeedUp = 100.0f;
		Post.bOverride_AutoExposureSpeedDown = true;
		Post.AutoExposureSpeedDown = 100.0f;
		Post.bOverride_MotionBlurAmount = true;
		Post.MotionBlurAmount = 0.0f;
		Post.bOverride_FilmGrainIntensity = true;
		Post.FilmGrainIntensity = 0.0f;
		Post.bOverride_VignetteIntensity = true;
		Post.VignetteIntensity = 0.0f;
		Post.bOverride_BloomIntensity = true;
		Post.BloomIntensity = 0.25f;
	}

	void WarmUpWorldTicks(UWorld* World, int32 TickCount)
	{
		if (!World)
		{
			return;
		}
		for (int32 Tick = 0; Tick < TickCount; ++Tick)
		{
			World->Tick(LEVELTICK_All, SimTickDelta);
		}
		FlushRenderingCommands();
	}

	void DestroyTrackedActors(TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		for (TWeakObjectPtr<AActor>& Actor : Actors)
		{
			if (Actor.IsValid())
			{
				Actor->Destroy();
			}
		}
	}

	bool TeardownComplete(const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		for (const TWeakObjectPtr<AActor>& Actor : Actors)
		{
			if (Actor.IsValid() && !Actor->IsActorBeingDestroyed())
			{
				return false;
			}
		}
		return true;
	}

	// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Kismet/KismetRenderingLibrary.h:44-48]
	UTextureRenderTarget2D* CreateCaptureTarget(
		UWorld* World,
		int32 Width,
		int32 Height)
	{
		return UKismetRenderingLibrary::CreateRenderTarget2D(
			World, Width, Height, RTF_RGBA8);
	}

	ASceneCapture2D* SpawnFramedCapture(
		UWorld* World,
		const FVector& CameraLocation,
		const FVector& AimAt,
		UTextureRenderTarget2D* Target,
		TArray<TWeakObjectPtr<AActor>>& Tracked,
		float FovDegrees)
	{
		if (!World || !Target)
		{
			return nullptr;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(
			CameraLocation, FRotator::ZeroRotator, SpawnParameters);
		if (!Camera)
		{
			return nullptr;
		}
		Tracked.Add(Camera);
		Camera->SetActorRotation((AimAt - CameraLocation).Rotation());
		USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
		Capture->TextureTarget = Target;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->FOVAngle = FovDegrees;
		ApplyPinnedExposure(Capture);
		return Camera;
	}
}
