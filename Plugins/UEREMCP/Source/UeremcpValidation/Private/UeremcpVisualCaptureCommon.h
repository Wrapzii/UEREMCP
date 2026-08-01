// UEREMCP — shared visual-capture primitives (WS-11).
// Reused by Niagara / world / material / animation capture paths.
#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "UObject/WeakObjectPtr.h"

class USceneCaptureComponent2D;

namespace UeremcpVisualCapture
{
	constexpr float SimTickDelta = 1.0f / 60.0f;
	constexpr int32 MinimumChangedLitPixels = 16;

	/** Far-from-content origin used by disposable stages (VISUAL_CAPTURE_PROTOCOL). */
	inline const FVector StageOrigin = FVector(49300.0, 0.0, 0.0);

	struct FFrameStats
	{
		double Mean = 0.0;
		int32 LitPixels = 0;
		double Max = 0.0;
	};

	FVector CameraOffset(const FString& Preset);

	/**
	 * Compute a focus box for CaptureWorldFrames from live actors (skips info /
	 * capture / volume noise). Returns false when the world has no useful bounds
	 * — caller may fall back to StageOrigin.
	 */
	bool ComputeWorldContentBounds(UWorld* World, FBox& OutBounds);

	/** Scale a three_quarter-style offset so the focus box fills the frame. */
	FVector ScaledCameraOffset(const FString& Preset, const FBox& FocusBounds);

	/** Saved/UEREMCP/<Domain>/<Asset>/<RequestId>/ with safe path components. */
	FString MakeOutputDirectory(
		const FString& Domain,
		const FString& AssetLabel,
		const FString& RequestId);

	bool IsPathUnderSavedUeremcp(const FString& AbsolutePath);

	bool ReadStats(UTextureRenderTarget2D* Target, FFrameStats& Out);

	bool IsNonEmptyPng(const FString& Path);

	/** Triple CaptureScene + PNG export + reread. [VERIFIED: KismetRenderingLibrary.h] */
	bool CaptureExportAndVerify(
		UWorld* World,
		USceneCaptureComponent2D* Capture,
		UTextureRenderTarget2D* Target,
		const FString& Directory,
		const FString& FileName,
		FFrameStats& OutStats,
		int32& InternalOperations);

	void ApplyPinnedExposure(USceneCaptureComponent2D* Capture);

	void WarmUpWorldTicks(UWorld* World, int32 TickCount);

	void DestroyTrackedActors(TArray<TWeakObjectPtr<AActor>>& Actors);

	bool TeardownComplete(const TArray<TWeakObjectPtr<AActor>>& Actors);

	UTextureRenderTarget2D* CreateCaptureTarget(
		UWorld* World,
		int32 Width,
		int32 Height);

	ASceneCapture2D* SpawnFramedCapture(
		UWorld* World,
		const FVector& CameraLocation,
		const FVector& AimAt,
		UTextureRenderTarget2D* Target,
		TArray<TWeakObjectPtr<AActor>>& Tracked,
		float FovDegrees = 52.0f);
}
