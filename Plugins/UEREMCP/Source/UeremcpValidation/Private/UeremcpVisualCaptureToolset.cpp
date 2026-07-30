// UEREMCP — deterministic Niagara visual verification (WS-11).
#include "UeremcpVisualCaptureToolset.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpEnvelope.h"
#include "UeremcpJobRegistry.h"
#include "UeremcpMutatingDispatch.h"

namespace
{
	const FVector StageOrigin(49300.0, 0.0, 0.0);
	constexpr float SimTickDelta = 1.0f / 60.0f;
	constexpr int32 MinimumChangedLitPixels = 16;

	struct FFrameStats
	{
		double Mean = 0.0;
		int32 LitPixels = 0;
		double Max = 0.0;
	};

	FVector CameraOffset(const FString& Preset)
	{
		if (Preset == TEXT("front")) { return FVector(-900.0, 0.0, 300.0); }
		if (Preset == TEXT("side")) { return FVector(0.0, -950.0, 300.0); }
		if (Preset == TEXT("top")) { return FVector(-300.0, 0.0, 1050.0); }
		return FVector(-820.0, -560.0, 360.0);
	}

	bool ReadStats(UTextureRenderTarget2D* Target, FFrameStats& Out)
	{
		if (!Target)
		{
			return false;
		}
		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
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
		return IsNonEmptyPng(Path);
	}

	void DestroySpawnedActors(TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		for (TWeakObjectPtr<AActor>& Actor : Actors)
		{
			if (Actor.IsValid())
			{
				Actor->Destroy();
			}
		}
	}

	bool ParseTerminalResponse(
		const FString& Json,
		FUeremcpResponse& OutResponse)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
			|| !Root->TryGetStringField(TEXT("status"), OutResponse.Status)
			|| !Root->TryGetStringField(TEXT("summary"), OutResponse.Summary))
		{
			return false;
		}

		Root->TryGetStringField(TEXT("protocol_version"), OutResponse.ProtocolVersion);
		Root->TryGetStringField(TEXT("request_id"), OutResponse.RequestId);
		const TSharedPtr<FJsonObject>* Metrics = nullptr;
		if (Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics
			&& Metrics->IsValid())
		{
			(*Metrics)->TryGetNumberField(
				TEXT("mcp_round_trips"), OutResponse.Metrics.McpRoundTrips);
			(*Metrics)->TryGetNumberField(
				TEXT("internal_operations"), OutResponse.Metrics.InternalOperations);
		}
		const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
		if (Root->TryGetArrayField(TEXT("capability_notes"), Notes) && Notes)
		{
			for (const TSharedPtr<FJsonValue>& Note : *Notes)
			{
				FString Value;
				if (Note.IsValid() && Note->TryGetString(Value))
				{
					OutResponse.CapabilityNotes.Add(Value);
				}
			}
		}

		// Preserve result/verification extensions that are not represented by
		// FUeremcpResponse's typed fields. SerializeResponse ignores duplicate
		// standard fields already emitted from the typed response.
		OutResponse.ExtraFields = Root;
		return true;
	}

	FString CaptureEffectFramesImpl(
		const FString& RequestJson,
		const bool bAllowColdRetry);
}

namespace
{
FString CaptureEffectFramesImpl(
	const FString& RequestJson,
	const bool bAllowColdRetry)
{
	FUeremcpMutatingDispatch Dispatch;
	FString BlockingResponse;
	// This operation writes under Saved/ and uses a shared transient stage origin,
	// so it takes the project mutator slot even though it never changes the target asset.
	if (!Dispatch.TryBegin(RequestJson, true, 0, false, BlockingResponse))
	{
		return BlockingResponse;
	}
	const FUeremcpRequest& Request = Dispatch.GetRequest();

	auto CompleteFailure = [&Dispatch, &Request](
		const FString& Status,
		const FString& Summary)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.Status = Status;
		Response.Summary = Summary;
		Response.UnderstoodAction = TEXT("capture_effect_frames");
		Response.UnderstoodTarget = Request.TargetAssetPath;
		Response.Metrics.McpRoundTrips = 1;
		return Dispatch.Complete(Response);
	};

	if (Request.Action != TEXT("capture_effect_frames"))
	{
		return CompleteFailure(TEXT("rejected"),
			TEXT("action must be capture_effect_frames"));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return CompleteFailure(TEXT("rejected"),
			TEXT("target.asset_path is required"));
	}
	if (!Request.bValidate)
	{
		return CompleteFailure(TEXT("rejected"),
			TEXT("capture_effect_frames requires options.validate=true"));
	}

	int32 FrameCount = 8;
	double DurationSeconds = 1.5;
	int32 Width = 960;
	int32 Height = 540;
	FString CameraPreset = TEXT("three_quarter");
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetNumberField(TEXT("frame_count"), FrameCount);
		Request.Specification->TryGetNumberField(
			TEXT("duration_seconds"), DurationSeconds);
		Request.Specification->TryGetNumberField(TEXT("width"), Width);
		Request.Specification->TryGetNumberField(TEXT("height"), Height);
		Request.Specification->TryGetStringField(TEXT("camera"), CameraPreset);
	}
	const TSet<FString> CameraPresets =
		{ TEXT("front"), TEXT("three_quarter"), TEXT("side"), TEXT("top") };
	if (FrameCount < 1 || FrameCount > 64
		|| DurationSeconds < 0.0 || DurationSeconds > 30.0
		|| Width < 64 || Width > 4096
		|| Height < 64 || Height > 4096
		|| !CameraPresets.Contains(CameraPreset))
	{
		return CompleteFailure(TEXT("rejected"),
			TEXT("invalid specification: frame_count 1..64, duration_seconds 0..30, "
				 "width/height 64..4096, camera front|three_quarter|side|top"));
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.Status = TEXT("partially_completed");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would capture %d frames over %.2f seconds."),
			FrameCount, DurationSeconds);
		Response.UnderstoodAction = TEXT("capture_effect_frames");
		Response.UnderstoodTarget = Request.TargetAssetPath;
		Response.Metrics.McpRoundTrips = 1;
		Response.CapabilityNotes.Add(TEXT("No stage or image files were created."));
		return Dispatch.Complete(Response);
	}

	UNiagaraSystem* System =
		LoadObject<UNiagaraSystem>(nullptr, *Request.TargetAssetPath);
	if (!System)
	{
		return CompleteFailure(TEXT("rejected"),
			FString::Printf(TEXT("could not load Niagara system: %s"),
				*Request.TargetAssetPath));
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !IsInGameThread())
	{
		return CompleteFailure(TEXT("error"),
			TEXT("capture requires the editor world on the game thread"));
	}

	const FString RunId = Request.RequestId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::Digits)
		: FPaths::MakeValidFileName(Request.RequestId);
	const FString AssetName =
		FPaths::MakeValidFileName(FPaths::GetBaseFilename(Request.TargetAssetPath));
	const FString OutputDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("UEREMCP"), TEXT("VfxCapture"),
		AssetName, RunId);
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		return CompleteFailure(TEXT("error"),
			FString::Printf(TEXT("could not create output directory: %s"),
				*OutputDirectory));
	}

	int32 InternalOperations = 0;
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
	auto Track = [&SpawnedActors](AActor* Actor)
	{
		if (Actor)
		{
			SpawnedActors.Add(Actor);
		}
		return Actor;
	};
	ON_SCOPE_EXIT
	{
		DestroySpawnedActors(SpawnedActors);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;

	UStaticMesh* Plane =
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane)
	{
		return CompleteFailure(TEXT("error"),
			TEXT("could not load /Engine/BasicShapes/Plane"));
	}
	const TArray<TPair<FVector, FRotator>> Backdrop = {
		{ StageOrigin, FRotator::ZeroRotator },
		{ StageOrigin + FVector(900.0, 0.0, 0.0),
			FRotator(-90.0, 0.0, 0.0) }
	};
	for (const TPair<FVector, FRotator>& Part : Backdrop)
	{
		AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(
			Part.Key, Part.Value, SpawnParameters);
		Track(Wall);
		if (!Wall)
		{
			return CompleteFailure(TEXT("error"),
				TEXT("failed to spawn transient backdrop"));
		}
		Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(Plane);
		Wall->SetActorScale3D(FVector(30.0, 30.0, 1.0));
		++InternalOperations;
	}

	ADirectionalLight* Key = World->SpawnActor<ADirectionalLight>(
		StageOrigin + FVector(0.0, 0.0, 1000.0),
		FRotator(-42.0, -140.0, 0.0), SpawnParameters);
	Track(Key);
	if (Key)
	{
		Key->GetComponent()->SetMobility(EComponentMobility::Movable);
		Key->GetComponent()->SetIntensity(2.0f);
		Key->GetComponent()->SetLightColor(
			FLinearColor(0.85f, 0.90f, 1.0f));
		++InternalOperations;
	}
	APointLight* Fill = World->SpawnActor<APointLight>(
		StageOrigin + FVector(-450.0, 420.0, 320.0),
		FRotator::ZeroRotator, SpawnParameters);
	Track(Fill);
	if (Fill)
	{
		UPointLightComponent* Light =
			CastChecked<UPointLightComponent>(Fill->GetLightComponent());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensity(8500.0f);
		Light->SetAttenuationRadius(3200.0f);
		Light->SetLightColor(FLinearColor(0.49f, 0.76f, 1.0f));
		++InternalOperations;
	}

	// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Kismet/KismetRenderingLibrary.h:44-48]
	UTextureRenderTarget2D* Target =
		UKismetRenderingLibrary::CreateRenderTarget2D(
			World, Width, Height, RTF_RGBA8);
	if (!Target)
	{
		return CompleteFailure(TEXT("error"), TEXT("render-target creation failed"));
	}

	const FVector CameraLocation = StageOrigin + CameraOffset(CameraPreset);
	ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(
		CameraLocation, FRotator::ZeroRotator, SpawnParameters);
	Track(Camera);
	if (!Camera)
	{
		return CompleteFailure(TEXT("error"),
			TEXT("scene-capture actor spawn failed"));
	}
	Camera->SetActorRotation(
		(StageOrigin + FVector(0.0, 0.0, 210.0) - CameraLocation).Rotation());
	USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
	Capture->TextureTarget = Target;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->FOVAngle = 52.0f;
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
	++InternalOperations;

	FFrameStats Baseline;
	if (!CaptureExportAndVerify(
			World, Capture, Target, OutputDirectory, TEXT("baseline.png"),
			Baseline, InternalOperations))
	{
		return CompleteFailure(TEXT("failed_validation"),
			TEXT("baseline capture was not exported as a non-empty PNG"));
	}
	if (Baseline.Max < 5.0)
	{
		return CompleteFailure(TEXT("failed_validation"),
			TEXT("baseline frame is black; pixel comparisons would be invalid"));
	}

	ANiagaraActor* Subject = World->SpawnActor<ANiagaraActor>(
		StageOrigin + FVector(0.0, 0.0, 60.0),
		FRotator::ZeroRotator, SpawnParameters);
	Track(Subject);
	if (!Subject)
	{
		return CompleteFailure(TEXT("error"), TEXT("Niagara actor spawn failed"));
	}
	UNiagaraComponent* NiagaraComponent = Subject->GetNiagaraComponent();
	NiagaraComponent->SetAsset(System);
	// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraComponent.h:306-307,650,659-661]
	NiagaraComponent->SetForceSolo(true);
	InternalOperations += 2;

	// Prime Niagara's render resources before measuring frame zero. A newly
	// loaded system can otherwise produce an entirely empty first series while
	// the same request immediately repeated renders correctly.
	NiagaraComponent->ReinitializeSystem();
	NiagaraComponent->Activate(true);
	NiagaraComponent->AdvanceSimulation(1, SimTickDelta);
	NiagaraComponent->MarkRenderDynamicDataDirty();
	World->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	for (int32 WarmupCapture = 0; WarmupCapture < 3; ++WarmupCapture)
	{
		Capture->CaptureScene();
	}
	InternalOperations += 9;

	TArray<TSharedPtr<FJsonValue>> Frames;
	int32 MaxDeltaLitPixels = 0;
	int32 CaptureAttempts = 0;
	for (int32 Attempt = 0; Attempt < 1; ++Attempt)
	{
		++CaptureAttempts;
		Frames.Reset();
		MaxDeltaLitPixels = 0;
		for (int32 Index = 0; Index < FrameCount; ++Index)
		{
			const double Age = FrameCount == 1
				? DurationSeconds
				: DurationSeconds * Index / static_cast<double>(FrameCount - 1);
			NiagaraComponent->Deactivate();
			NiagaraComponent->ReinitializeSystem();
			NiagaraComponent->Activate(true);
			InternalOperations += 3;
			if (Age > 0.0)
			{
				const int32 Ticks =
					FMath::Max(1, FMath::RoundToInt(Age / SimTickDelta));
				NiagaraComponent->AdvanceSimulation(Ticks, SimTickDelta);
				++InternalOperations;
			}
			// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h:1131]
			// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Engine/World.h:3474]
			// [VERIFIED: Engine/Source/Runtime/RenderCore/Public/RenderingThread.h:111]
			NiagaraComponent->MarkRenderDynamicDataDirty();
			World->SendAllEndOfFrameUpdates();
			FlushRenderingCommands();
			InternalOperations += 3;

			const FString FileName =
				FString::Printf(TEXT("age_%02d.png"), Index);
			FFrameStats Stats;
			if (!CaptureExportAndVerify(
					World, Capture, Target, OutputDirectory, FileName,
					Stats, InternalOperations))
			{
				return CompleteFailure(TEXT("failed_validation"),
					FString::Printf(
						TEXT("frame %d was not exported as a non-empty PNG"),
						Index));
			}

			const int32 DeltaLitPixels =
				Stats.LitPixels - Baseline.LitPixels;
			MaxDeltaLitPixels =
				FMath::Max(MaxDeltaLitPixels, FMath::Abs(DeltaLitPixels));
			const TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
			Frame->SetNumberField(TEXT("index"), Index);
			Frame->SetNumberField(TEXT("age_seconds"), Age);
			Frame->SetStringField(
				TEXT("image"), FPaths::Combine(OutputDirectory, FileName));
			Frame->SetNumberField(TEXT("mean_luminance"), Stats.Mean);
			Frame->SetNumberField(TEXT("lit_pixels"), Stats.LitPixels);
			Frame->SetNumberField(TEXT("max_luminance"), Stats.Max);
			Frame->SetNumberField(TEXT("delta_lit_pixels"), DeltaLitPixels);
			Frames.Add(MakeShared<FJsonValueObject>(Frame));
		}
		if (MaxDeltaLitPixels >= MinimumChangedLitPixels)
		{
			break;
		}
	}

	DestroySpawnedActors(SpawnedActors);
	const bool bTeardownComplete = SpawnedActors.ContainsByPredicate(
		[](const TWeakObjectPtr<AActor>& Actor)
		{
			return Actor.IsValid() && !Actor->IsActorBeingDestroyed();
		}) == false;
	// A small effect can legitimately cover only tens of pixels at this camera
	// distance. Sixteen suppresses isolated readback noise without rejecting the
	// known IceWall control.
	// [VERIFIED-RUNTIME: CaptureEffectFrames IceWall control produced 44 changed
	// lit pixels at 640x360 on RE, 2026-07-30]
	const bool bRenderedSomething =
		MaxDeltaLitPixels >= MinimumChangedLitPixels;

	if (!bRenderedSomething && bAllowColdRetry)
	{
		FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();
		FString JobId;
		FString JobError;
		if (!Registry.CreateJob(
				Request.RequestId,
				false,
				TEXT("Cold Niagara renderer detected; retrying after an editor tick."),
				JobId,
				JobError)
			|| !Registry.StartJob(JobId, JobError))
		{
			return CompleteFailure(
				TEXT("failed_validation"),
				FString::Printf(
					TEXT("no material pixel change and cold-retry scheduling failed: %s"),
					*JobError));
		}

		const FString RetryRequest = RequestJson;
		const int32 FirstAttemptOperations = InternalOperations;
		// [VERIFIED: Engine/Source/Runtime/Core/Public/Containers/Ticker.h:81]
		FTSTicker::GetCoreTicker().AddTicker(
			TEXT("UEREMCP.VisualCaptureColdRetry"),
			0.25f,
			[JobId, RetryRequest, FirstAttemptOperations](float)
			{
				FString ProgressError;
				FUeremcpJobRegistry::Get().UpdateProgress(
					JobId,
					0.5,
					TEXT("Capturing after the editor tick boundary."),
					ProgressError);

				const FString TerminalJson =
					CaptureEffectFramesImpl(RetryRequest, false);
				FUeremcpResponse TerminalResponse;
				if (!ParseTerminalResponse(TerminalJson, TerminalResponse))
				{
					FString FailureError;
					FUeremcpJobRegistry::Get().FailJob(
						JobId,
						TEXT("Cold visual-capture retry returned an invalid envelope."),
						FailureError);
					return false;
				}
				TerminalResponse.Metrics.InternalOperations += FirstAttemptOperations;
				TerminalResponse.CapabilityNotes.Add(
					TEXT("A cold first capture required one ADR-0009 editor-tick "
						 "retry; this terminal result includes both attempts."));
				FString CompletionError;
				if (!FUeremcpJobRegistry::Get().CompleteJob(
						JobId, TerminalResponse, CompletionError))
				{
					FString FailureError;
					FUeremcpJobRegistry::Get().FailJob(
						JobId,
						FString::Printf(
							TEXT("Cold visual-capture retry could not complete: %s"),
							*CompletionError),
						FailureError);
				}
				return false;
			});

		FUeremcpResponse InFlight;
		if (!Registry.GetTimeoutResponse(JobId, InFlight, JobError))
		{
			return CompleteFailure(
				TEXT("error"),
				FString::Printf(
					TEXT("cold-retry job could not be read: %s"), *JobError));
		}
		InFlight.UnderstoodAction = TEXT("capture_effect_frames");
		InFlight.UnderstoodTarget = Request.TargetAssetPath;
		InFlight.PrimaryAsset = Request.TargetAssetPath;
		InFlight.Metrics.InternalOperations = InternalOperations;
		InFlight.CapabilityNotes.Add(
			TEXT("Poll get_job_result; the capture retry runs after a short editor-"
				 "tick warmup and is not cancellable."));
		return Dispatch.Complete(InFlight);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = bRenderedSomething && bTeardownComplete
		? TEXT("no_change_required")
		: TEXT("failed_validation");
	Response.Summary = FString::Printf(
		TEXT("Captured and reread %d PNG frames over %.2f seconds; %s"),
		FrameCount, DurationSeconds,
		bRenderedSomething
			? TEXT("pixels changed against baseline")
			: TEXT("no material pixel change against baseline"));
	Response.UnderstoodAction = TEXT("capture_effect_frames");
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = InternalOperations;
	Response.CapabilityNotes.Add(
		TEXT("This verifies non-empty PNG export and pixel change only; it does not "
			 "judge appearance or prove Niagara compile validity."));
	Response.CapabilityNotes.Add(
		TEXT("Output is written under Project/Saved/UEREMCP/VfxCapture; no content "
			 "asset is created or modified."));
	Response.CapabilityNotes.Add(
		TEXT("A cold renderer is retried once after an editor tick through an "
			 "ADR-0009 process-local job; a second zero-pixel result remains "
			 "failed_validation."));

	const TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetStringField(TEXT("output_directory"), OutputDirectory);
	Extra->SetStringField(TEXT("camera"), CameraPreset);
	Extra->SetArrayField(TEXT("frames"), Frames);
	const TSharedRef<FJsonObject> Verification = MakeShared<FJsonObject>();
	Verification->SetBoolField(TEXT("png_files_reread"), true);
	Verification->SetBoolField(TEXT("rendered_something"), bRenderedSomething);
	Verification->SetBoolField(TEXT("stage_teardown_complete"), bTeardownComplete);
	Verification->SetNumberField(
		TEXT("max_delta_lit_pixels"), MaxDeltaLitPixels);
	Verification->SetNumberField(
		TEXT("minimum_changed_lit_pixels"), MinimumChangedLitPixels);
	Verification->SetNumberField(TEXT("capture_attempts"), CaptureAttempts);
	Verification->SetNumberField(
		TEXT("baseline_lit_pixels"), Baseline.LitPixels);
	Extra->SetObjectField(TEXT("verification"), Verification);
	Response.ExtraFields = Extra;
	return Dispatch.Complete(Response);
}
}

FString UUeremcpVisualCaptureToolset::CaptureEffectFrames(
	const FString& RequestJson)
{
	return CaptureEffectFramesImpl(RequestJson, true);
}
