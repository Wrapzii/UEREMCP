#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "EditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "LevelEditorViewport.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "UnrealClient.h"

namespace UeremcpValidation::PocB10
{
	static constexpr TCHAR FireballPath[] = TEXT(
		"/Game/__UeremcpPoc/NS_POCB_Fireball.NS_POCB_Fireball");
	static constexpr int32 MinimumChangedPixels = 100;
	static constexpr int32 MinimumWarmChangedPixels = 20;
	static constexpr int32 MinimumWarmupFrames = 30;
	static constexpr double WarmupSeconds = 1.5;

	static FString ResolveSystemPath()
	{
		FString SystemPath;
		if (FParse::Value(
				FCommandLine::Get(),
				TEXT("UeremcpPocB10System="),
				SystemPath)
			&& !SystemPath.IsEmpty())
		{
			return SystemPath;
		}
		return FireballPath;
	}

	static AStaticMeshActor* CreateDarkBackdrop(UWorld& World)
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Plane.Plane"));
		UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Plane || !BaseMaterial)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		AStaticMeshActor* Backdrop = World.SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			FVector(150.0, 0.0, 0.0),
			FRotator(90.0, 0.0, 0.0),
			SpawnParameters);
		if (!Backdrop || !Backdrop->GetStaticMeshComponent())
		{
			return nullptr;
		}

		UStaticMeshComponent* MeshComponent = Backdrop->GetStaticMeshComponent();
		// The plane is transient test scenery, not user content.
		Backdrop->SetActorScale3D(FVector(20.0));
		// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Components/StaticMeshComponent.h:448-450]
		MeshComponent->SetStaticMesh(Plane);

		// BasicShapeMaterial exposes its tint as a vector parameter. Set every vector
		// parameter dark so the test does not depend on the parameter's display name.
		// [VERIFIED-RUNTIME: B10 canary rendered warm pixels over the resulting dark backdrop]
		// [VERIFIED: Engine/Source/Runtime/Engine/Public/Materials/MaterialInterface.h:949-952]
		// [VERIFIED: Engine/Source/Runtime/Engine/Public/Materials/MaterialInstanceDynamic.h:107-109,173-176]
		UMaterialInstanceDynamic* DarkMaterial =
			UMaterialInstanceDynamic::Create(BaseMaterial, MeshComponent);
		if (!DarkMaterial)
		{
			Backdrop->Destroy();
			return nullptr;
		}
		TArray<FMaterialParameterInfo> VectorParameters;
		TArray<FGuid> VectorParameterIds;
		BaseMaterial->GetAllVectorParameterInfo(VectorParameters, VectorParameterIds);
		for (const FMaterialParameterInfo& Parameter : VectorParameters)
		{
			DarkMaterial->SetVectorParameterValue(
				Parameter.Name,
				FLinearColor(0.005f, 0.005f, 0.005f, 1.0f));
		}
		// [VERIFIED: Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h:1600]
		MeshComponent->SetMaterial(/*ElementIndex=*/0, DarkMaterial);
		return Backdrop;
	}

	static int32 CountActiveParticles(UNiagaraComponent& Component)
	{
		const FNiagaraSystemInstanceControllerPtr Controller =
			Component.GetSystemInstanceController();
		if (!Controller.IsValid())
		{
			return -1;
		}

		// Forced-solo instances are explicitly safe to inspect from the game thread.
		// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraSystemInstanceController.h:82-88]
		FNiagaraSystemInstance* Instance = Controller->GetSoloSystemInstance();
		if (!Instance)
		{
			return -1;
		}

		int32 ParticleCount = 0;
		// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraSystemInstance.h:277-278]
		// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraEmitterInstance.h:71-73]
		for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
		{
			ParticleCount += Emitter->GetNumParticles();
		}
		return ParticleCount;
	}

	struct FVisibleRenderState
	{
		UWorld* World = nullptr;
		UNiagaraSystem* System = nullptr;
		FEditorViewportClient* ViewportClient = nullptr;
		FViewport* Viewport = nullptr;
		ANiagaraActor* NiagaraActor = nullptr;
		AStaticMeshActor* Backdrop = nullptr;
		UNiagaraComponent* Component = nullptr;
		FVector PreviousLocation = FVector::ZeroVector;
		FRotator PreviousRotation = FRotator::ZeroRotator;
		ELevelViewportType PreviousType = LVT_Perspective;
		FText RealtimeOverride;
		FString SystemPath;
		bool bPreviousParticlesShowFlag = false;
		bool bPreviousNiagaraShowFlag = false;
		TArray<FColor> BeforePixels;
		FIntPoint BeforeSize = FIntPoint::ZeroValue;
		bool bCapturedBefore = false;
		int32 WarmupFrames = 0;
		double WarmupElapsedSeconds = 0.0;
	};

	static FString ResolveOutputPath()
	{
		FString OutputPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("UeremcpPocB10Output="), OutputPath)
			&& !OutputPath.IsEmpty())
		{
			return OutputPath;
		}

		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("UEREMCP"),
			TEXT("poc_b10_fireball.png"));
	}

	static bool CaptureViewport(
		FViewport& Viewport,
		TArray<FColor>& OutPixels,
		FIntPoint& OutSize)
	{
		// [VERIFIED: Engine/Source/Runtime/Engine/Public/UnrealClient.h:582]
		Viewport.Draw(/*bShouldPresent=*/true);
		OutSize = Viewport.GetSizeXY();
		// [VERIFIED: Engine/Source/Runtime/Engine/Public/UnrealClient.h:113]
		return OutSize.X > 0 && OutSize.Y > 0 && Viewport.ReadPixels(OutPixels);
	}

	static bool SavePng(
		const FString& OutputPath,
		const FIntPoint Size,
		const TArray<FColor>& Pixels)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), /*Tree=*/true);
		TArray64<uint8> Compressed;
		// [VERIFIED: Engine/Source/Runtime/Engine/Public/ImageUtils.h:334]
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Compressed);
		// [VERIFIED: Engine/Source/Runtime/Core/Public/Misc/FileHelper.h:190]
		return Compressed.Num() > 0 && FFileHelper::SaveArrayToFile(Compressed, *OutputPath);
	}

	static void CountVisibleDifference(
		const TArray<FColor>& Before,
		const TArray<FColor>& After,
		int32& OutChanged,
		int32& OutWarmChanged)
	{
		OutChanged = 0;
		OutWarmChanged = 0;
		const int32 Count = FMath::Min(Before.Num(), After.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FColor& A = Before[Index];
			const FColor& B = After[Index];
			const int32 Delta =
				FMath::Abs(static_cast<int32>(A.R) - B.R)
				+ FMath::Abs(static_cast<int32>(A.G) - B.G)
				+ FMath::Abs(static_cast<int32>(A.B) - B.B);
			if (Delta < 30)
			{
				continue;
			}

			++OutChanged;
			const bool bWarm =
				A.R <= 80 && A.G <= 80 && A.B <= 80
				&& B.R >= 96
				&& static_cast<int32>(B.R) >= static_cast<int32>(B.G) + 20
				&& static_cast<int32>(B.G) >= static_cast<int32>(B.B) + 5;
			if (bWarm)
			{
				++OutWarmChanged;
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBVisibleRender,
	"UEREMCP.Niagara.POCB.VisibleRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FUeremcpPocB10BaselineCommand final : public IAutomationLatentCommand
{
public:
	explicit FUeremcpPocB10BaselineCommand(
		TSharedRef<UeremcpValidation::PocB10::FVisibleRenderState> InState)
		: State(MoveTemp(InState))
	{
	}

	virtual bool Update() override
	{
		using namespace UeremcpValidation::PocB10;
		State->Viewport->Invalidate();
		if (++SettleFrames < 5)
		{
			return false;
		}

		State->bCapturedBefore = CaptureViewport(
			*State->Viewport,
			State->BeforePixels,
			State->BeforeSize);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		State->NiagaraActor = State->World->SpawnActor<ANiagaraActor>(
			ANiagaraActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
		State->Component = State->NiagaraActor
			? State->NiagaraActor->GetNiagaraComponent()
			: nullptr;
		if (State->Component)
		{
			// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraComponent.h:295,307]
			State->Component->SetAsset(State->System);
			State->Component->SetForceSolo(true);
			State->Component->Activate(/*bReset=*/true);
		}
		return true;
	}

private:
	TSharedRef<UeremcpValidation::PocB10::FVisibleRenderState> State;
	int32 SettleFrames = 0;
};

class FUeremcpPocB10WarmupCommand final : public IAutomationLatentCommand
{
public:
	explicit FUeremcpPocB10WarmupCommand(
		TSharedRef<UeremcpValidation::PocB10::FVisibleRenderState> InState)
		: State(MoveTemp(InState))
	{
	}

	virtual bool Update() override
	{
		using namespace UeremcpValidation::PocB10;
		if (StartSeconds < 0.0)
		{
			StartSeconds = FPlatformTime::Seconds();
		}

		++State->WarmupFrames;
		State->WarmupElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;
		State->Viewport->Invalidate();

		// AddRealtimeOverride makes this a realtime editor viewport. Between latent
		// command updates, UEditorEngine advances its world with
		// LEVELTICK_ViewportsOnly and renders normally; this avoids manually
		// re-entering engine/task processing from an automation callback.
		// [VERIFIED: Engine/Source/Editor/UnrealEd/Private/EditorEngine.cpp:1956-1968]
		// [VERIFIED: Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h:525,4087]
		return State->WarmupFrames >= MinimumWarmupFrames
			&& State->WarmupElapsedSeconds >= WarmupSeconds;
	}

private:
	TSharedRef<UeremcpValidation::PocB10::FVisibleRenderState> State;
	double StartSeconds = -1.0;
};

class FUeremcpPocB10FinalizeCommand final : public IAutomationLatentCommand
{
public:
	FUeremcpPocB10FinalizeCommand(
		FAutomationTestBase* InTest,
		TSharedRef<UeremcpValidation::PocB10::FVisibleRenderState> InState)
		: Test(InTest)
		, State(MoveTemp(InState))
	{
	}

	virtual bool Update() override
	{
		using namespace UeremcpValidation::PocB10;

		TArray<FColor> AfterPixels;
		FIntPoint AfterSize = FIntPoint::ZeroValue;
		const bool bCapturedAfter =
			CaptureViewport(*State->Viewport, AfterPixels, AfterSize);

		const FString OutputPath = ResolveOutputPath();
		const bool bSavedScreenshot =
			bCapturedAfter && SavePng(OutputPath, AfterSize, AfterPixels);
		const int32 ParticleCount =
			State->Component ? CountActiveParticles(*State->Component) : 0;

		int32 ChangedPixels = 0;
		int32 WarmChangedPixels = 0;
		if (State->bCapturedBefore
			&& bCapturedAfter
			&& State->BeforeSize == AfterSize)
		{
			CountVisibleDifference(
				State->BeforePixels,
				AfterPixels,
				ChangedPixels,
				WarmChangedPixels);
		}

		if (State->NiagaraActor)
		{
			State->NiagaraActor->Destroy();
		}
		State->Backdrop->Destroy();
		State->ViewportClient->SetViewLocation(State->PreviousLocation);
		State->ViewportClient->SetViewRotation(State->PreviousRotation);
		State->ViewportClient->SetViewportType(State->PreviousType);
		State->ViewportClient->EngineShowFlags.SetParticles(
			State->bPreviousParticlesShowFlag);
		State->ViewportClient->EngineShowFlags.SetNiagara(
			State->bPreviousNiagaraShowFlag);
		// [VERIFIED: Engine/Source/Editor/UnrealEd/Public/EditorViewportClient.h:389]
		State->ViewportClient->RemoveRealtimeOverride(
			State->RealtimeOverride,
			/*bCheckMissingOverride=*/false);
		State->Viewport->Invalidate();

		const bool bVisible =
			State->bCapturedBefore
			&& bCapturedAfter
			&& State->BeforeSize == AfterSize
			&& bSavedScreenshot
			&& ChangedPixels >= MinimumChangedPixels
			&& WarmChangedPixels >= MinimumWarmChangedPixels;

		const TCHAR* FailureReason = TEXT("visible_fire_signature_not_observed");
		if (!State->bCapturedBefore
			|| !bCapturedAfter
			|| State->BeforeSize != AfterSize
			|| !bSavedScreenshot)
		{
			FailureReason = TEXT("viewport_unavailable");
		}
		else if (ParticleCount == 0)
		{
			FailureReason = TEXT("system_emits_no_particles");
		}

		Test->AddInfo(*FString::Printf(
			TEXT("UEREMCP_POC_B10_EVIDENCE={\"status\":\"%s\",\"screenshot\":\"%s\",")
			TEXT("\"width\":%d,\"height\":%d,\"changed_pixels\":%d,")
			TEXT("\"warm_changed_pixels\":%d,\"particle_count\":%d,")
			TEXT("\"warmup_frames\":%d,\"warmup_seconds\":%.3f,")
			TEXT("\"system\":\"%s\",\"dark_backdrop\":true,")
			TEXT("\"programmatic_pixel_validation\":true}"),
			bVisible ? TEXT("pass") : TEXT("fail"),
			*OutputPath.ReplaceCharWithEscapedChar(),
			AfterSize.X,
			AfterSize.Y,
			ChangedPixels,
			WarmChangedPixels,
			ParticleCount,
			State->WarmupFrames,
			State->WarmupElapsedSeconds,
			*State->SystemPath.ReplaceCharWithEscapedChar()));
		if (bVisible)
		{
			Test->AddInfo(TEXT(
				"UEREMCP_POC_B10_OUTCOME=PASS "
				"proof=viewport_pixel_delta_with_fire_signature"));
		}
		else
		{
			Test->AddInfo(*FString::Printf(
				TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=%s"),
				FailureReason));
		}

		Test->TestTrue(TEXT("before viewport pixels captured"), State->bCapturedBefore);
		Test->TestTrue(TEXT("after viewport pixels captured"), bCapturedAfter);
		Test->TestEqual(
			TEXT("viewport size remained stable"),
			State->BeforeSize,
			AfterSize);
		Test->TestTrue(TEXT("supplementary screenshot saved"), bSavedScreenshot);
		Test->TestTrue(
			TEXT("viewport changed after fireball placement"),
			ChangedPixels >= MinimumChangedPixels);
		Test->TestTrue(
			TEXT("warm fire signature visible"),
			WarmChangedPixels >= MinimumWarmChangedPixels);
		return true;
	}

private:
	FAutomationTestBase* Test;
	TSharedRef<UeremcpValidation::PocB10::FVisibleRenderState> State;
};

bool FUeremcpNiagaraPocBVisibleRender::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidation::PocB10;

	if (!FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=viewport_unavailable"));
		return false;
	}

	const FString SystemPath = ResolveSystemPath();
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!TestNotNull(TEXT("POC B fireball exists"), System))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=fireball_missing"));
		return false;
	}

	if (!TestNotNull(TEXT("editor is available"), GEditor)
		|| !TestNotNull(TEXT("active level viewport is available"), GCurrentLevelEditingViewportClient))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=viewport_unavailable"));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	FEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	// [VERIFIED: Engine/Source/Editor/UnrealEd/Public/EditorViewportClient.h:1962]
	FViewport* Viewport = ViewportClient->Viewport;
	if (!TestNotNull(TEXT("editor world is available"), World)
		|| !TestNotNull(TEXT("active viewport render target is available"), Viewport))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=viewport_unavailable"));
		return false;
	}

	const TSharedRef<FVisibleRenderState> State = MakeShared<FVisibleRenderState>();
	State->World = World;
	State->System = System;
	State->ViewportClient = ViewportClient;
	State->Viewport = Viewport;
	State->PreviousLocation = ViewportClient->GetViewLocation();
	State->PreviousRotation = ViewportClient->GetViewRotation();
	State->PreviousType = ViewportClient->GetViewportType();
	State->RealtimeOverride =
		FText::FromString(TEXT("UEREMCP POC B10 visible render"));
	State->SystemPath = SystemPath;
	State->bPreviousParticlesShowFlag =
		ViewportClient->EngineShowFlags.Particles != 0;
	State->bPreviousNiagaraShowFlag =
		ViewportClient->EngineShowFlags.Niagara != 0;

	// [VERIFIED: Engine/Source/Editor/UnrealEd/Public/EditorViewportClient.h:375]
	ViewportClient->AddRealtimeOverride(
		/*bShouldBeRealtime=*/true,
		State->RealtimeOverride);
	ViewportClient->SetViewportType(LVT_Perspective);
	ViewportClient->SetViewLocation(FVector(-250.0, 0.0, 0.0));
	ViewportClient->SetViewRotation(FRotator::ZeroRotator);
	// [VERIFIED: Engine/Source/Runtime/Engine/Public/ShowFlagsValues.inl:200-203]
	ViewportClient->EngineShowFlags.SetParticles(true);
	ViewportClient->EngineShowFlags.SetNiagara(true);
	Viewport->Invalidate();

	State->Backdrop = CreateDarkBackdrop(*World);
	if (!TestNotNull(TEXT("dark transient backdrop is available"), State->Backdrop))
	{
		ViewportClient->SetViewLocation(State->PreviousLocation);
		ViewportClient->SetViewRotation(State->PreviousRotation);
		ViewportClient->SetViewportType(State->PreviousType);
		ViewportClient->EngineShowFlags.SetParticles(
			State->bPreviousParticlesShowFlag);
		ViewportClient->EngineShowFlags.SetNiagara(
			State->bPreviousNiagaraShowFlag);
		ViewportClient->RemoveRealtimeOverride(
			State->RealtimeOverride,
			/*bCheckMissingOverride=*/false);
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=viewport_unavailable"));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FUeremcpPocB10BaselineCommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FUeremcpPocB10WarmupCommand(State));
	ADD_LATENT_AUTOMATION_COMMAND(FUeremcpPocB10FinalizeCommand(this, State));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
