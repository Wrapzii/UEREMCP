// UEREMCP — UI goal-op dispatch (create / host / capture / preview / inventory).

#include "UeremcpUIToolset.h"
#include "UeremcpUIService.h"

#include "UeremcpEnvelope.h"
#include "UeremcpSecurityDomainAdoption.h"
#include "UeremcpSecurityTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EngineUtils.h"
#include "ILevelEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SLevelViewport.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Animation/SkeletalMeshActor.h"
#include "Slate/SceneViewport.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	bool CommonPreamble(
		const FString& RequestJson,
		const TCHAR* ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutRejection)
	{
		FString ParseError;
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, ParseError))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				FString(), FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
					*OutRequest.ProtocolVersion, *FUeremcpEnvelope::ProtocolVersion()));
			return false;
		}
		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("%s tool received action '%s'."),
					ExpectedAction, *OutRequest.Action));
			return false;
		}
		return true;
	}

	bool OptionsFlag(const FUeremcpRequest& Request, const TCHAR* Key, bool Default)
	{
		if (FCString::Stricmp(Key, TEXT("save")) == 0)
		{
			return Request.bSave;
		}
		if (FCString::Stricmp(Key, TEXT("compile")) == 0)
		{
			return Request.bCompile;
		}
		return Default;
	}

	FString SpecAssetPath(const FUeremcpRequest& Request, const TSharedPtr<FJsonObject>& Spec)
	{
		FString Path;
		if (Spec.IsValid())
		{
			Spec->TryGetStringField(TEXT("asset_path"), Path);
			if (Path.IsEmpty())
			{
				Spec->TryGetStringField(TEXT("widget_blueprint"), Path);
			}
			if (Path.IsEmpty())
			{
				Spec->TryGetStringField(TEXT("widget_asset"), Path);
			}
		}
		if (Path.IsEmpty())
		{
			Path = Request.TargetAssetPath;
		}
		return Path;
	}

	bool ValidateGameWrite(const FString& AssetPath, FString& OutError)
	{
		const FUeremcpPathValidationResult Decision =
			FUeremcpSecurityDomainAdoption::ValidateWriteSoftPath(AssetPath);
		if (!Decision.bAllowed)
		{
			OutError = Decision.Reason.IsEmpty()
				? FString::Printf(TEXT("Write denied for '%s'"), *AssetPath)
				: Decision.Reason;
			return false;
		}
		return true;
	}

	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	void DestroyActorsByLabel(UWorld* World, const FString& Label)
	{
		if (!World || Label.IsEmpty())
		{
			return;
		}
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase))
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			World->DestroyActor(Actor);
		}
	}

	FTransform ParseTransform(const TSharedPtr<FJsonObject>& Spec)
	{
		FVector Loc(200.f, -150.f, 120.f);
		FRotator Rot(0.f, 140.f, 0.f);
		FVector Scale(0.25f, 0.25f, 0.25f);
		const TSharedPtr<FJsonObject>* Xf = nullptr;
		if (Spec.IsValid() && Spec->TryGetObjectField(TEXT("transform"), Xf) && Xf)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if ((*Xf)->TryGetArrayField(TEXT("location"), Arr) && Arr && Arr->Num() >= 3)
			{
				Loc = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
			}
			if ((*Xf)->TryGetArrayField(TEXT("rotation"), Arr) && Arr && Arr->Num() >= 3)
			{
				Rot = FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
			}
			if ((*Xf)->TryGetArrayField(TEXT("scale"), Arr) && Arr && Arr->Num() >= 3)
			{
				Scale = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
			}
			else if ((*Xf)->TryGetArrayField(TEXT("scale"), Arr) && Arr && Arr->Num() == 1)
			{
				const double S = (*Arr)[0]->AsNumber();
				Scale = FVector(S, S, S);
			}
		}
		return FTransform(Rot, Loc, Scale);
	}

	FVector2D ParseDrawSize(const TSharedPtr<FJsonObject>& Spec)
	{
		FVector2D Size(1920.f, 1080.f);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("draw_size"), Arr) && Arr && Arr->Num() >= 2)
		{
			Size = FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());
		}
		return Size;
	}

	UWidgetBlueprint* LoadWBP(const FString& AssetPath, FString& OutError)
	{
		bool bCreated = false;
		return FUeremcpUIService::LoadOrCreateWidgetBlueprint(
			AssetPath, FString(), false, false, OutError, bCreated);
	}

	AActor* SpawnUIHost(
		UWorld* World,
		const FString& Label,
		const FTransform& Xform,
		UClass* WidgetClass,
		const FVector2D& DrawSize,
		EWidgetSpace Space,
		FString& OutError)
	{
		if (!World)
		{
			OutError = TEXT("No editor world.");
			return nullptr;
		}
		if (!WidgetClass)
		{
			OutError = TEXT("Widget GeneratedClass is null — compile the WBP first.");
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), Xform, Params);
		if (!Host)
		{
			OutError = TEXT("Failed to spawn UI host actor.");
			return nullptr;
		}
		Host->SetActorLabel(Label);
		Host->SetActorTransform(Xform);

		USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("Root"));
		Host->SetRootComponent(Root);
		Host->AddInstanceComponent(Root);
		Root->RegisterComponent();

		UWidgetComponent* WC = NewObject<UWidgetComponent>(Host, TEXT("UIWidget"));
		WC->SetupAttachment(Root);
		Host->AddInstanceComponent(WC);
		WC->RegisterComponent();
		WC->SetWidgetSpace(Space);
		WC->SetDrawSize(DrawSize);
		WC->SetPivot(FVector2D(0.5f, 0.5f));
		WC->SetTwoSided(true);
		WC->SetWidgetClass(WidgetClass);
		WC->RequestRedraw();
		return Host;
	}

	bool CaptureEditorViewportPng(const FString& AbsolutePath, FString& OutError)
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<ILevelEditor> LevelEd = LevelEditor.GetFirstLevelEditor();
		if (!LevelEd.IsValid())
		{
			OutError = TEXT("No LevelEditor.");
			return false;
		}
		TSharedPtr<SLevelViewport> ViewportWidget = LevelEd->GetActiveViewportInterface();
		if (!ViewportWidget.IsValid())
		{
			const TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEd->GetViewports();
			if (Viewports.Num() > 0)
			{
				ViewportWidget = Viewports[0];
			}
		}
		if (!ViewportWidget.IsValid() || !ViewportWidget->GetActiveViewport())
		{
			OutError = TEXT("No active level viewport.");
			return false;
		}

		FViewport* Viewport = ViewportWidget->GetActiveViewport();
		Viewport->Draw(true);
		TArray<FColor> Pixels;
		const FIntPoint Size = Viewport->GetSizeXY();
		if (Size.X <= 0 || Size.Y <= 0 || !Viewport->ReadPixels(Pixels))
		{
			OutError = TEXT("Viewport ReadPixels failed.");
			return false;
		}

		IImageWrapperModule& ImageWrapperModule =
			FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (!Png.IsValid() || !Png->SetRaw(
			Pixels.GetData(), Pixels.Num() * sizeof(FColor),
			Size.X, Size.Y, ERGBFormat::BGRA, 8))
		{
			OutError = TEXT("PNG encode failed.");
			return false;
		}
		const TArray64<uint8>& Compressed = Png->GetCompressed();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		if (!FFileHelper::SaveArrayToFile(Compressed, *AbsolutePath))
		{
			OutError = FString::Printf(TEXT("Failed to write %s"), *AbsolutePath);
			return false;
		}
		return true;
	}

	void FocusActors(UWorld* World, const TArray<FString>& Labels)
	{
		if (!World || Labels.Num() == 0)
		{
			return;
		}
		FBox Bound(ForceInit);
		bool bAny = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			for (const FString& Label : Labels)
			{
				if (It->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase))
				{
					Bound += It->GetComponentsBoundingBox(true);
					bAny = true;
					break;
				}
			}
		}
		if (!bAny)
		{
			return;
		}
		if (GCurrentLevelEditingViewportClient)
		{
			GCurrentLevelEditingViewportClient->FocusViewportOnBox(Bound);
		}
	}
}

FString UUeremcpUIToolset::CreateWidgetFromSpec(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("create_widget_from_spec"), Request, Rejection))
	{
		return Rejection;
	}

	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	const FString AssetPath = SpecAssetPath(Request, Spec);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("create_widget_from_spec requires specification.asset_path."));
	}
	FString PathError;
	if (!ValidateGameWrite(AssetPath, PathError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, PathError);
	}

	FString ThemeId = TEXT("northridge_fantasy");
	FUeremcpUITheme Theme = FUeremcpUIService::ThemeFromId(ThemeId);
	if (Spec.IsValid())
	{
		if (Spec->HasField(TEXT("theme")))
		{
			const TSharedPtr<FJsonObject>* ThemeObj = nullptr;
			if (Spec->TryGetObjectField(TEXT("theme"), ThemeObj) && ThemeObj)
			{
				Theme = FUeremcpUIService::ThemeFromJson(*ThemeObj, ThemeId);
			}
			else
			{
				Spec->TryGetStringField(TEXT("theme"), ThemeId);
				Theme = FUeremcpUIService::ThemeFromId(ThemeId);
			}
		}
	}

	FString UiFramework = TEXT("umg");
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("ui_framework"), UiFramework);
	}
	if (UiFramework.Equals(TEXT("common_ui"), ESearchCase::IgnoreCase))
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(TEXT("specification.ui_framework"), TEXT("umg"));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("CommonUI is not available in this project path — use ui_framework=umg."),
			TEXT("COMMON_UI_UNAVAILABLE"),
			Next);
	}

	FString ParentClass = TEXT("/Script/UMG.UserWidget");
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("parent_class"), ParentClass);
	}

	const bool bSave = OptionsFlag(Request, TEXT("save"), true);
	const bool bCompile = OptionsFlag(Request, TEXT("compile"), true);

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would create WBP %s with theme %s."), *AssetPath, *Theme.Id);
		Response.Metrics.McpRoundTrips = 1;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString CreateError;
	bool bCreated = false;
	UWidgetBlueprint* WBP = FUeremcpUIService::LoadOrCreateWidgetBlueprint(
		AssetPath, ParentClass, true, false, CreateError, bCreated);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, CreateError);
	}

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("WBP has no WidgetTree."));
	}

	// Rebuild root.
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Tree->RootWidget = Root;

	UWidgetSwitcher* Switcher = Tree->ConstructWidget<UWidgetSwitcher>(
		UWidgetSwitcher::StaticClass(), TEXT("ScreenSwitcher"));
	if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Switcher))
	{
		Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		Slot->SetOffsets(FMargin(0.f));
	}

	TArray<FString> WidgetNames;
	WidgetNames.Add(TEXT("RootCanvas"));
	WidgetNames.Add(TEXT("ScreenSwitcher"));
	TArray<FString> ScreenIds;

	const TArray<TSharedPtr<FJsonValue>>* Screens = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("screens"), Screens) && Screens && Screens->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& ScreenVal : *Screens)
		{
			const TSharedPtr<FJsonObject> Screen = ScreenVal->AsObject();
			if (!Screen.IsValid())
			{
				continue;
			}
			FString ScreenId = TEXT("main");
			Screen->TryGetStringField(TEXT("id"), ScreenId);
			ScreenIds.Add(ScreenId);
			UCanvasPanel* ScreenRoot = Tree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), FName(*FString::Printf(TEXT("Screen_%s"), *ScreenId)));
			Switcher->AddChild(ScreenRoot);
			WidgetNames.Add(ScreenRoot->GetName());
			FUeremcpUIService::BuildOverlayLeftPanel(
				Tree, ScreenRoot, ScreenId, Screen, Theme, WidgetNames);
		}
	}
	else
	{
		// Minimal default screen.
		TSharedPtr<FJsonObject> DefaultScreen = MakeShared<FJsonObject>();
		DefaultScreen->SetStringField(TEXT("id"), TEXT("main"));
		TSharedPtr<FJsonObject> Brand = MakeShared<FJsonObject>();
		Brand->SetStringField(TEXT("title"), TEXT("NORTHRIDGE ONLINE"));
		Brand->SetStringField(TEXT("tagline"), TEXT("Forge your legend"));
		DefaultScreen->SetObjectField(TEXT("brand"), Brand);
		TArray<TSharedPtr<FJsonValue>> Actions;
		Actions.Add(MakeShared<FJsonValueString>(TEXT("Play")));
		Actions.Add(MakeShared<FJsonValueString>(TEXT("Quit")));
		DefaultScreen->SetArrayField(TEXT("actions"), Actions);
		ScreenIds.Add(TEXT("main"));
		UCanvasPanel* ScreenRoot = Tree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("Screen_main"));
		Switcher->AddChild(ScreenRoot);
		FUeremcpUIService::BuildOverlayLeftPanel(
			Tree, ScreenRoot, TEXT("main"), DefaultScreen, Theme, WidgetNames);
	}
	Switcher->SetActiveWidgetIndex(0);

	FString SaveError;
	if (!FUeremcpUIService::CompileAndOptionallySave(WBP, bCompile, bSave, SaveError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, SaveError);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = bCreated ? TEXT("created_with_warnings") : TEXT("modified_and_validated");
	Response.PrimaryAsset = AssetPath;
	Response.Summary = FString::Printf(
		TEXT("%s WBP %s (%d widgets, %d screens, theme=%s)%s."),
		bCreated ? TEXT("Created") : TEXT("Rebuilt"),
		*AssetPath, WidgetNames.Num(), ScreenIds.Num(), *Theme.Id,
		bSave ? TEXT(", saved") : TEXT(", NOT saved"));
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.AssetsAffected = 1;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("theme_id"), Theme.Id);
	Result->SetNumberField(TEXT("widget_count"), WidgetNames.Num());
	Result->SetBoolField(TEXT("saved"), bSave);
	Result->SetBoolField(TEXT("compiled"), bCompile);
	TArray<TSharedPtr<FJsonValue>> ScreensJson;
	for (const FString& Id : ScreenIds)
	{
		ScreensJson.Add(MakeShared<FJsonValueString>(Id));
	}
	Result->SetArrayField(TEXT("screens"), ScreensJson);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	Response.CapabilityNotes.Add(
		TEXT("Navigation graph stubs are not authored yet — use UMGToolSet.BindToEventProperty for OnClicked."));
	Response.CapabilityNotes.Add(
		TEXT("Default options.save=true so the WBP survives editor crash."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::ApplyUiTheme(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("apply_ui_theme"), Request, Rejection))
	{
		return Rejection;
	}
	const FString AssetPath = SpecAssetPath(Request, Request.Specification);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("apply_ui_theme requires specification.widget_blueprint."));
	}

	FUeremcpUITheme Theme = FUeremcpUIService::ThemeFromId(TEXT("northridge_fantasy"));
	if (Request.Specification.IsValid())
	{
		const TSharedPtr<FJsonObject>* ThemeObj = nullptr;
		if (Request.Specification->TryGetObjectField(TEXT("theme"), ThemeObj) && ThemeObj)
		{
			Theme = FUeremcpUIService::ThemeFromJson(*ThemeObj, Theme.Id);
		}
		else
		{
			FString ThemeId;
			if (Request.Specification->TryGetStringField(TEXT("theme"), ThemeId))
			{
				Theme = FUeremcpUIService::ThemeFromId(ThemeId);
			}
		}
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would apply theme %s to %s."), *Theme.Id, *AssetPath);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	UWidgetBlueprint* WBP = LoadWBP(AssetPath, Err);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}
	int32 Styled = 0;
	FUeremcpUIService::ApplyThemeToTree(WBP, Theme, Styled);
	const bool bSave = OptionsFlag(Request, TEXT("save"), true);
	if (!FUeremcpUIService::CompileAndOptionallySave(WBP, true, bSave, Err))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("modified_and_validated");
	Response.PrimaryAsset = AssetPath;
	Response.Summary = FString::Printf(TEXT("Applied theme %s to %d widgets on %s."), *Theme.Id, Styled, *AssetPath);
	Response.Metrics.McpRoundTrips = 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("theme_id"), Theme.Id);
	Result->SetNumberField(TEXT("styled_widgets"), Styled);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::ShowWidgetInWorld(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("show_widget_in_world"), Request, Rejection))
	{
		return Rejection;
	}
	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	const FString AssetPath = SpecAssetPath(Request, Spec);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("show_widget_in_world requires specification.widget_asset."));
	}
	FString HostLabel = TEXT("UEREMCP_UIHost");
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("host_label"), HostLabel);
	}
	const FTransform Xform = ParseTransform(Spec);
	const FVector2D DrawSize = ParseDrawSize(Spec);

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would host %s on world WC labeled %s."), *AssetPath, *HostLabel);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	UWidgetBlueprint* WBP = LoadWBP(AssetPath, Err);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}
	if (!WBP->GeneratedClass)
	{
		FKismetEditorUtilities::CompileBlueprint(WBP);
	}
	if (!WBP->GeneratedClass)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("WBP GeneratedClass still null after compile."));
	}

	UWorld* World = EditorWorld();
	DestroyActorsByLabel(World, HostLabel);
	AActor* Host = SpawnUIHost(
		World, HostLabel, Xform, WBP->GeneratedClass, DrawSize, EWidgetSpace::World, Err);
	if (!Host)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Hosted %s on world-space WidgetComponent (%s). Simulate then CaptureViewport / capture_ui_frame."),
		*AssetPath, *HostLabel);
	Response.Metrics.McpRoundTrips = 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("host_actor"), Host->GetPathName());
	Result->SetStringField(TEXT("host_label"), HostLabel);
	Result->SetStringField(TEXT("space"), TEXT("World"));
	Result->SetStringField(TEXT("widget_asset"), AssetPath);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	Response.CapabilityNotes.Add(
		TEXT("World-space WC is the preferred diegetic / CaptureViewport-visible MMO overlay path."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::ShowWidgetOnScreen(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("show_widget_on_screen"), Request, Rejection))
	{
		return Rejection;
	}
	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	const FString AssetPath = SpecAssetPath(Request, Spec);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("show_widget_on_screen requires specification.widget_asset."));
	}
	FString HostLabel = TEXT("UEREMCP_UIHost_Screen");
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("host_label"), HostLabel);
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would host %s as screen WC."), *AssetPath);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	UWidgetBlueprint* WBP = LoadWBP(AssetPath, Err);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}
	if (!WBP->GeneratedClass)
	{
		FKismetEditorUtilities::CompileBlueprint(WBP);
	}

	UWorld* World = EditorWorld();
	DestroyActorsByLabel(World, HostLabel);
	const FTransform Xform = FTransform::Identity;
	AActor* Host = SpawnUIHost(
		World, HostLabel, Xform, WBP->GeneratedClass, ParseDrawSize(Spec), EWidgetSpace::Screen, Err);
	if (!Host)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Hosted %s as Screen-space WidgetComponent (%s). CaptureViewport will NOT see this — use world host or CaptureEditorImage."),
		*AssetPath, *HostLabel);
	Response.Metrics.McpRoundTrips = 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("host_actor"), Host->GetPathName());
	Result->SetStringField(TEXT("host_label"), HostLabel);
	Result->SetStringField(TEXT("space"), TEXT("Screen"));
	Result->SetBoolField(TEXT("capture_viewport_includes_ui"), false);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	Response.CapabilityNotes.Add(
		TEXT("bShowUI on CaptureViewport does not composite screen-space game UMG. Prefer show_widget_in_world for proof."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::CaptureUiFrame(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("capture_ui_frame"), Request, Rejection))
	{
		return Rejection;
	}
	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	FString RelPath = TEXT("Saved/UEREMCP/UI/capture.png");
	bool bWorld = true;
	bool bWorldWidgets = true;
	bool bScreenUmg = false;
	bool bEditorChrome = false;
	TArray<FString> FocusLabels;
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("path"), RelPath);
		const TSharedPtr<FJsonObject>* Include = nullptr;
		if (Spec->TryGetObjectField(TEXT("include"), Include) && Include)
		{
			(*Include)->TryGetBoolField(TEXT("world"), bWorld);
			(*Include)->TryGetBoolField(TEXT("world_space_widgets"), bWorldWidgets);
			(*Include)->TryGetBoolField(TEXT("screen_space_umg"), bScreenUmg);
			(*Include)->TryGetBoolField(TEXT("editor_chrome"), bEditorChrome);
		}
		const TSharedPtr<FJsonObject>* Camera = nullptr;
		if (Spec->TryGetObjectField(TEXT("camera"), Camera) && Camera)
		{
			const TArray<TSharedPtr<FJsonValue>>* Focus = nullptr;
			if ((*Camera)->TryGetArrayField(TEXT("focus_actors"), Focus) && Focus)
			{
				for (const TSharedPtr<FJsonValue>& V : *Focus)
				{
					FocusLabels.Add(V->AsString());
				}
			}
		}
	}

	if (bScreenUmg)
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetBoolField(TEXT("specification.include.screen_space_umg"), false);
		Next->SetBoolField(TEXT("specification.include.world_space_widgets"), true);
		Next->SetStringField(TEXT("next_action"), TEXT("show_widget_in_world"));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("Screen-space game UMG cannot be composited into CaptureViewport. "
				 "Use show_widget_in_world (preferred MMO path) or CaptureEditorImage for editor chrome."),
			TEXT("SCREEN_UMG_CAPTURE_UNSUPPORTED"),
			Next);
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would capture viewport to %s."), *RelPath);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FocusActors(EditorWorld(), FocusLabels);

	FString AbsPath = RelPath;
	if (FPaths::IsRelative(AbsPath))
	{
		AbsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelPath);
	}

	FString Err;
	if (!CaptureEditorViewportPng(AbsPath, Err))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Captured level viewport (world + world-space widgets) to %s. Screen UMG excluded."),
		*AbsPath);
	Response.Metrics.McpRoundTrips = 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("path"), AbsPath);
	Result->SetBoolField(TEXT("includes_world_space_widgets"), bWorldWidgets);
	Result->SetBoolField(TEXT("includes_screen_space_umg"), false);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	Response.CapabilityNotes.Add(
		TEXT("World-space WidgetComponents draw as scene geometry and appear in this capture once Simulate/view is framed."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::SpawnCharacterPreview(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("spawn_character_preview"), Request, Rejection))
	{
		return Rejection;
	}
	const TSharedPtr<FJsonObject> Spec = Request.Specification;
	FString Label = TEXT("UEREMCP_CharPreview");
	FVector Location(80.f, 40.f, 0.f);
	float Yaw = -30.f;
	bool bRequireVisible = true;
	FString MaterialPath;
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("label"), Label);
		Spec->TryGetStringField(TEXT("material"), MaterialPath);
		Spec->TryGetBoolField(TEXT("require_visible"), bRequireVisible);
		const TArray<TSharedPtr<FJsonValue>>* Loc = nullptr;
		if (Spec->TryGetArrayField(TEXT("location"), Loc) && Loc && Loc->Num() >= 3)
		{
			Location = FVector((*Loc)[0]->AsNumber(), (*Loc)[1]->AsNumber(), (*Loc)[2]->AsNumber());
		}
		Spec->TryGetNumberField(TEXT("rotation_yaw"), Yaw);
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would spawn preview %s."), *Label);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	UWorld* World = EditorWorld();
	if (!World)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("No editor world."));
	}
	DestroyActorsByLabel(World, Label);

	FString MeshSource = TEXT("proxy");
	FString MeshPathUsed;
	USkeletalMesh* Skel = nullptr;

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& Registry = ARM.Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		const FString Name = Asset.AssetName.ToString();
		if (Name.Contains(TEXT("Mannequin")) || Name.Contains(TEXT("Manny")) || Name.Contains(TEXT("Quinn")))
		{
			Skel = Cast<USkeletalMesh>(Asset.GetAsset());
			if (Skel)
			{
				MeshSource = TEXT("project");
				MeshPathUsed = Asset.GetObjectPathString();
				break;
			}
		}
	}
	if (!Skel)
	{
		Assets.Reset();
		Filter.PackagePaths.Reset();
		Filter.PackagePaths.Add(TEXT("/Engine"));
		Registry.GetAssets(Filter, Assets);
		for (const FAssetData& Asset : Assets)
		{
			const FString Name = Asset.AssetName.ToString();
			if (Name.Contains(TEXT("Mannequin")) || Name.Contains(TEXT("TutorialTPP")))
			{
				Skel = Cast<USkeletalMesh>(Asset.GetAsset());
				if (Skel)
				{
					MeshSource = TEXT("engine");
					MeshPathUsed = Asset.GetObjectPathString();
					break;
				}
			}
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Preview = nullptr;

	if (Skel)
	{
		ASkeletalMeshActor* SkelActor = World->SpawnActor<ASkeletalMeshActor>(
			Location, FRotator(0.f, Yaw, 0.f), Params);
		if (SkelActor)
		{
			SkelActor->GetSkeletalMeshComponent()->SetSkeletalMesh(Skel);
			SkelActor->SetActorLabel(Label);
			Preview = SkelActor;
		}
	}

	if (!Preview)
	{
		MeshSource = TEXT("proxy");
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		AStaticMeshActor* Proxy = World->SpawnActor<AStaticMeshActor>(
			Location, FRotator(0.f, Yaw, 0.f), Params);
		if (!Proxy || !Cylinder)
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				TEXT("Failed to spawn character preview proxy."),
				TEXT("PREVIEW_NOT_VISIBLE"),
				nullptr);
		}
		Proxy->GetStaticMeshComponent()->SetStaticMesh(Cylinder);
		Proxy->SetActorScale3D(FVector(0.45f, 0.45f, 1.1f));
		Proxy->SetActorLabel(Label);
		MeshPathUsed = TEXT("/Engine/BasicShapes/Cylinder");
		Preview = Proxy;
	}

	if (!MaterialPath.IsEmpty())
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
		{
			if (ASkeletalMeshActor* Sk = Cast<ASkeletalMeshActor>(Preview))
			{
				Sk->GetSkeletalMeshComponent()->SetMaterial(0, Mat);
			}
			else if (AStaticMeshActor* Sm = Cast<AStaticMeshActor>(Preview))
			{
				Sm->GetStaticMeshComponent()->SetMaterial(0, Mat);
			}
		}
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Spawned character preview %s (mesh_source=%s)."), *Label, *MeshSource);
	Response.Metrics.McpRoundTrips = 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("actor_label"), Label);
	Result->SetStringField(TEXT("actor_path"), Preview->GetPathName());
	Result->SetStringField(TEXT("mesh_source"), MeshSource);
	Result->SetStringField(TEXT("mesh_path"), MeshPathUsed);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	if (MeshSource.Equals(TEXT("proxy")))
	{
		Response.CapabilityNotes.Add(
			TEXT("No project/engine mannequin found — used cylinder proxy. Import a mannequin under /Game for skeletal previews."));
	}
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::SaveWidgetAsset(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("save_widget_asset"), Request, Rejection))
	{
		return Rejection;
	}
	const FString AssetPath = SpecAssetPath(Request, Request.Specification);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("save_widget_asset requires specification.widget_blueprint."));
	}
	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would save %s."), *AssetPath);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}
	FString Err;
	UWidgetBlueprint* WBP = LoadWBP(AssetPath, Err);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}
	if (!FUeremcpUIService::CompileAndOptionallySave(WBP, false, true, Err))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}
	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("modified_and_validated");
	Response.PrimaryAsset = AssetPath;
	Response.Summary = FString::Printf(TEXT("Saved Widget Blueprint %s to disk."), *AssetPath);
	Response.Metrics.McpRoundTrips = 1;
	return FUeremcpEnvelope::SerializeResponse(Response);
}
