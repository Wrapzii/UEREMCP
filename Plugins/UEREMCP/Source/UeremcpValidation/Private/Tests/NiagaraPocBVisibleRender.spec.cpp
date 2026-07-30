#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UnrealClient.h"

namespace UeremcpValidation::PocB10
{
	static constexpr TCHAR FireballPath[] = TEXT(
		"/Game/__UeremcpPoc/NS_POCB_Fireball.NS_POCB_Fireball");
	static constexpr int32 MinimumChangedPixels = 100;
	static constexpr int32 MinimumWarmChangedPixels = 20;

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
				B.R >= 80
				&& static_cast<int32>(B.R) * 5 >= static_cast<int32>(B.G) * 6
				&& static_cast<int32>(B.R) * 3 >= static_cast<int32>(B.B) * 4;
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

bool FUeremcpNiagaraPocBVisibleRender::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidation::PocB10;

	if (!FApp::CanEverRender() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=SKIP reason=rendering_unavailable"));
		return true;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, FireballPath);
	if (!TestNotNull(TEXT("POC B fireball exists"), System))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=fireball_missing"));
		return false;
	}

	if (!TestNotNull(TEXT("editor is available"), GEditor)
		|| !TestNotNull(TEXT("active level viewport is available"), GCurrentLevelEditingViewportClient))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=SKIP reason=editor_viewport_unavailable"));
		return true;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	FEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	FViewport* Viewport = ViewportClient->GetViewport();
	if (!TestNotNull(TEXT("editor world is available"), World)
		|| !TestNotNull(TEXT("active viewport render target is available"), Viewport))
	{
		AddInfo(TEXT("UEREMCP_POC_B10_OUTCOME=SKIP reason=editor_world_or_viewport_unavailable"));
		return true;
	}

	const FVector PreviousLocation = ViewportClient->GetViewLocation();
	const FRotator PreviousRotation = ViewportClient->GetViewRotation();
	const ELevelViewportType PreviousType = ViewportClient->GetViewportType();
	const FText RealtimeOverride = FText::FromString(TEXT("UEREMCP POC B10 visible render"));

	// [VERIFIED: Engine/Source/Editor/UnrealEd/Public/EditorViewportClient.h:375]
	ViewportClient->AddRealtimeOverride(/*bShouldBeRealtime=*/true, RealtimeOverride);
	ViewportClient->SetViewportType(LVT_Perspective);
	ViewportClient->SetViewLocation(FVector(-250.0, 0.0, 0.0));
	ViewportClient->SetViewRotation(FRotator::ZeroRotator);
	Viewport->Invalidate();

	TArray<FColor> BeforePixels;
	FIntPoint BeforeSize = FIntPoint::ZeroValue;
	const bool bCapturedBefore = CaptureViewport(*Viewport, BeforePixels, BeforeSize);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	ANiagaraActor* Actor = World->SpawnActor<ANiagaraActor>(
		ANiagaraActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	UNiagaraComponent* Component = Actor ? Actor->GetNiagaraComponent() : nullptr;
	if (Component)
	{
		// [VERIFIED: Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraComponent.h:295,307,661]
		Component->SetAsset(System);
		Component->SetForceSolo(true);
		Component->Activate(/*bReset=*/true);
		Component->AdvanceSimulation(/*TickCount=*/120, /*TickDeltaSeconds=*/1.0f / 60.0f);
		Component->MarkRenderStateDirty();
	}

	Viewport->Invalidate();
	TArray<FColor> AfterPixels;
	FIntPoint AfterSize = FIntPoint::ZeroValue;
	const bool bCapturedAfter = CaptureViewport(*Viewport, AfterPixels, AfterSize);

	const FString OutputPath = ResolveOutputPath();
	const bool bSavedScreenshot =
		bCapturedAfter && SavePng(OutputPath, AfterSize, AfterPixels);

	int32 ChangedPixels = 0;
	int32 WarmChangedPixels = 0;
	if (bCapturedBefore && bCapturedAfter && BeforeSize == AfterSize)
	{
		CountVisibleDifference(
			BeforePixels,
			AfterPixels,
			ChangedPixels,
			WarmChangedPixels);
	}

	if (Actor)
	{
		Actor->Destroy();
	}
	ViewportClient->SetViewLocation(PreviousLocation);
	ViewportClient->SetViewRotation(PreviousRotation);
	ViewportClient->SetViewportType(PreviousType);
	// [VERIFIED: Engine/Source/Editor/UnrealEd/Public/EditorViewportClient.h:389]
	ViewportClient->RemoveRealtimeOverride(RealtimeOverride, /*bCheckMissingOverride=*/false);
	Viewport->Invalidate();

	const bool bVisible =
		bCapturedBefore
		&& bCapturedAfter
		&& BeforeSize == AfterSize
		&& bSavedScreenshot
		&& ChangedPixels >= MinimumChangedPixels
		&& WarmChangedPixels >= MinimumWarmChangedPixels;

	AddInfo(*FString::Printf(
		TEXT("UEREMCP_POC_B10_EVIDENCE={\"status\":\"%s\",\"screenshot\":\"%s\",")
		TEXT("\"width\":%d,\"height\":%d,\"changed_pixels\":%d,")
		TEXT("\"warm_changed_pixels\":%d,\"programmatic_pixel_validation\":true}"),
		bVisible ? TEXT("pass") : TEXT("fail"),
		*OutputPath.ReplaceCharWithEscapedChar(),
		AfterSize.X,
		AfterSize.Y,
		ChangedPixels,
		WarmChangedPixels));
	AddInfo(bVisible
		? TEXT("UEREMCP_POC_B10_OUTCOME=PASS proof=viewport_pixel_delta_with_fire_signature")
		: TEXT("UEREMCP_POC_B10_OUTCOME=FAIL reason=visible_fire_signature_not_observed"));

	TestTrue(TEXT("before viewport pixels captured"), bCapturedBefore);
	TestTrue(TEXT("after viewport pixels captured"), bCapturedAfter);
	TestEqual(TEXT("viewport size remained stable"), BeforeSize, AfterSize);
	TestTrue(TEXT("supplementary screenshot saved"), bSavedScreenshot);
	TestTrue(TEXT("viewport changed after fireball placement"), ChangedPixels >= MinimumChangedPixels);
	TestTrue(TEXT("warm fire signature visible"), WarmChangedPixels >= MinimumWarmChangedPixels);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
