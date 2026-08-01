// UEREMCP — inventory sheet / slot / weight / layout goal ops.

#include "UeremcpUIToolset.h"
#include "UeremcpUIService.h"

#include "UeremcpEnvelope.h"
#include "UeremcpSecurityDomainAdoption.h"
#include "UeremcpSecurityTypes.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

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

	FString SpecPath(const FUeremcpRequest& Request)
	{
		FString Path;
		if (Request.Specification.IsValid())
		{
			Request.Specification->TryGetStringField(TEXT("asset_path"), Path);
			if (Path.IsEmpty())
			{
				Request.Specification->TryGetStringField(TEXT("widget_blueprint"), Path);
			}
		}
		if (Path.IsEmpty())
		{
			Path = Request.TargetAssetPath;
		}
		return Path;
	}

	FString CapitalizeId(const FString& Id)
	{
		if (Id.IsEmpty())
		{
			return Id;
		}
		FString Out = Id;
		Out[0] = FChar::ToUpper(Out[0]);
		return Out;
	}
}

FString UUeremcpUIToolset::CreateInventorySheetFromSpec(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("create_inventory_sheet_from_spec"), Request, Rejection))
	{
		return Rejection;
	}

	const FString AssetPath = SpecPath(Request);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_inventory_sheet_from_spec requires specification.asset_path."));
	}

	const FUeremcpPathValidationResult PathCheck =
		FUeremcpSecurityDomainAdoption::ValidateWriteSoftPath(AssetPath);
	if (!PathCheck.bAllowed)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, PathCheck.Reason);
	}

	FString ThemeId = TEXT("northridge_diegetic");
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("theme"), ThemeId);
	}
	const FUeremcpUITheme Theme = FUeremcpUIService::ThemeFromId(ThemeId);
	const bool bSave = OptionsFlag(Request, TEXT("save"), true);
	const bool bCompile = OptionsFlag(Request, TEXT("compile"), true);

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would create inventory sheet %s."), *AssetPath);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	bool bCreated = false;
	UWidgetBlueprint* WBP = FUeremcpUIService::LoadOrCreateWidgetBlueprint(
		AssetPath, TEXT("/Script/UMG.UserWidget"), true, false, Err, bCreated);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	UWidgetTree* Tree = WBP->WidgetTree;
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Tree->RootWidget = Root;

	UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SheetPanel"));
	FUeremcpUIService::ApplyPanelBorder(Panel, Theme);
	if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Panel))
	{
		Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		Slot->SetOffsets(FMargin(24.f));
	}

	UVerticalBox* VBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SheetVBox"));
	Panel->SetContent(VBox);

	UTextBlock* Header = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_CharacterName"));
	Header->SetText(FText::FromString(TEXT("CHARACTER SHEET")));
	FUeremcpUIService::ApplyTextStyle(Header, Theme, true);
	VBox->AddChildToVerticalBox(Header);

	UTextBlock* ClassLine = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_ClassLine"));
	ClassLine->SetText(FText::FromString(TEXT("CLASS | Level --")));
	FUeremcpUIService::ApplyTextStyle(ClassLine, Theme, false);
	VBox->AddChildToVerticalBox(ClassLine);

	int32 SlotCount = 0;
	TArray<FString> CreatedSlots;
	TArray<FString> GridIds;

	const TArray<TSharedPtr<FJsonValue>>* Grids = nullptr;
	if (!Request.Specification.IsValid()
		|| !Request.Specification->TryGetArrayField(TEXT("grids"), Grids)
		|| !Grids || Grids->Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_inventory_sheet_from_spec requires specification.grids[]."));
	}

	for (const TSharedPtr<FJsonValue>& GridVal : *Grids)
	{
		const TSharedPtr<FJsonObject> Grid = GridVal->AsObject();
		if (!Grid.IsValid())
		{
			continue;
		}
		FString GridId = TEXT("grid");
		FString Kind = TEXT("uniform_grid");
		Grid->TryGetStringField(TEXT("id"), GridId);
		Grid->TryGetStringField(TEXT("kind"), Kind);
		GridIds.Add(GridId);
		const FString CapId = CapitalizeId(GridId);

		UTextBlock* Section = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("Txt_%sHeader"), *CapId)));
		Section->SetText(FText::FromString(CapId.ToUpper()));
		FUeremcpUIService::ApplyTextStyle(Section, Theme, false);
		if (UVerticalBoxSlot* VSlot = VBox->AddChildToVerticalBox(Section))
		{
			VSlot->SetPadding(FMargin(0, 12, 0, 4));
		}

		UTextBlock* Weight = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("Txt_%sWeight"), *CapId)));
		Weight->SetText(FText::FromString(TEXT("0.0 / 0 KG")));
		FUeremcpUIService::ApplyTextStyle(Weight, Theme, false);
		VBox->AddChildToVerticalBox(Weight);

		if (Kind.Equals(TEXT("hotbar"), ESearchCase::IgnoreCase))
		{
			int32 Slots = 10;
			bool bShowIndex = true;
			Grid->TryGetNumberField(TEXT("slots"), Slots);
			Grid->TryGetBoolField(TEXT("show_index"), bShowIndex);
			if (Slots <= 0 || Slots > 20)
			{
				TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
				Next->SetNumberField(TEXT("specification.grids[].slots"), 10);
				return FUeremcpEnvelope::MakeRejection(
					Request.RequestId,
					TEXT("hotbar slots must be 1..20"),
					TEXT("UNKNOWN"),
					Next);
			}
			UHorizontalBox* Hotbar = Tree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("Hotbar_%s"), *CapId)));
			VBox->AddChildToVerticalBox(Hotbar);
			static const TCHAR* DefaultLabels[] = {
				TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"),
				TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0")
			};
			for (int32 i = 0; i < Slots; ++i)
			{
				const FString SlotName = FString::Printf(TEXT("Slot_Hotbar_%d"), i);
				const FString Idx = bShowIndex
					? (i < 10 ? FString(DefaultLabels[i]) : FString::FromInt(i + 1))
					: FString();
				FUeremcpUIService::BuildSlotCell(Tree, Hotbar, SlotName, 64, Theme, Idx);
				CreatedSlots.Add(SlotName);
				++SlotCount;
			}
		}
		else if (Kind.Equals(TEXT("paper_doll"), ESearchCase::IgnoreCase))
		{
			UVerticalBox* Doll = Tree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("PaperDoll_%s"), *CapId)));
			VBox->AddChildToVerticalBox(Doll);
			const TArray<TSharedPtr<FJsonValue>>* SlotsArr = nullptr;
			TArray<FString> EquipSlots;
			if (Grid->TryGetArrayField(TEXT("slots"), SlotsArr) && SlotsArr)
			{
				for (const TSharedPtr<FJsonValue>& S : *SlotsArr)
				{
					EquipSlots.Add(S->AsString());
				}
			}
			if (EquipSlots.Num() == 0)
			{
				EquipSlots = { TEXT("head"), TEXT("neck"), TEXT("torso"), TEXT("arms"), TEXT("legs"), TEXT("feet") };
			}
			for (const FString& Equip : EquipSlots)
			{
				const FString SlotName = FString::Printf(TEXT("Slot_Equip_%s"), *CapitalizeId(Equip));
				FUeremcpUIService::BuildSlotCell(Tree, Doll, SlotName, 72, Theme, Equip.ToUpper());
				CreatedSlots.Add(SlotName);
				++SlotCount;
			}
		}
		else if (Kind.Equals(TEXT("uniform_grid"), ESearchCase::IgnoreCase))
		{
			int32 Rows = 0;
			int32 Cols = 0;
			int32 CellPx = 72;
			Grid->TryGetNumberField(TEXT("rows"), Rows);
			Grid->TryGetNumberField(TEXT("cols"), Cols);
			Grid->TryGetNumberField(TEXT("cell_px"), CellPx);
			if (Rows <= 0 || Cols <= 0 || Rows > 16 || Cols > 16)
			{
				TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
				Next->SetNumberField(TEXT("specification.grids[].rows"), 4);
				Next->SetNumberField(TEXT("specification.grids[].cols"), 5);
				return FUeremcpEnvelope::MakeRejection(
					Request.RequestId,
					TEXT("uniform_grid requires rows/cols in 1..16"),
					TEXT("UNKNOWN"),
					Next);
			}
			UUniformGridPanel* GridPanel = Tree->ConstructWidget<UUniformGridPanel>(
				UUniformGridPanel::StaticClass(), FName(*FString::Printf(TEXT("Grid_%s"), *CapId)));
			VBox->AddChildToVerticalBox(GridPanel);
			for (int32 R = 0; R < Rows; ++R)
			{
				for (int32 C = 0; C < Cols; ++C)
				{
					const FString SlotName = FString::Printf(
						TEXT("Slot_%s_r%d_c%d"), *CapId, R, C);
					FUeremcpUIService::BuildSlotCell(
						Tree, GridPanel, SlotName, CellPx, Theme, FString(), R, C);
					CreatedSlots.Add(SlotName);
					++SlotCount;
				}
			}
		}
		else
		{
			TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
			Next->SetStringField(TEXT("specification.grids[].kind"), TEXT("uniform_grid|hotbar|paper_doll"));
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(TEXT("Unknown grid kind '%s'."), *Kind),
				TEXT("UNKNOWN"),
				Next);
		}
	}

	if (!FUeremcpUIService::CompileAndOptionallySave(WBP, bCompile, bSave, Err))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = bCreated ? TEXT("created_with_warnings") : TEXT("modified_and_validated");
	Response.PrimaryAsset = AssetPath;
	Response.Summary = FString::Printf(
		TEXT("%s inventory sheet %s (%d slots across %d grids)%s."),
		bCreated ? TEXT("Created") : TEXT("Rebuilt"),
		*AssetPath, SlotCount, GridIds.Num(),
		bSave ? TEXT(", saved") : TEXT(""));
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.AssetsAffected = 1;
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetNumberField(TEXT("slot_count"), SlotCount);
	TArray<TSharedPtr<FJsonValue>> SlotsJson;
	for (const FString& S : CreatedSlots)
	{
		SlotsJson.Add(MakeShared<FJsonValueString>(S));
	}
	Result->SetArrayField(TEXT("slots"), SlotsJson);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::SetSlotIcon(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("set_slot_icon"), Request, Rejection))
	{
		return Rejection;
	}
	const FString AssetPath = SpecPath(Request);
	FString SlotName;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("slot"), SlotName);
	}
	if (AssetPath.IsEmpty() || SlotName.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("set_slot_icon requires widget_blueprint and slot."));
	}

	FString TexturePath;
	if (Request.Specification.IsValid())
	{
		const TSharedPtr<FJsonObject>* Icon = nullptr;
		if (Request.Specification->TryGetObjectField(TEXT("icon"), Icon) && Icon)
		{
			(*Icon)->TryGetStringField(TEXT("texture"), TexturePath);
			if (TexturePath.IsEmpty())
			{
				FString Atlas;
				FString Name;
				(*Icon)->TryGetStringField(TEXT("atlas"), Atlas);
				(*Icon)->TryGetStringField(TEXT("name"), Name);
				if (!Atlas.IsEmpty() && !Name.IsEmpty())
				{
					// Named atlas cells are not yet a separate asset API — treat name as soft path hint.
					TexturePath = Atlas;
					TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
					Next->SetStringField(TEXT("specification.icon.texture"), Atlas);
					return FUeremcpEnvelope::MakeRejection(
						Request.RequestId,
						TEXT("Named atlas cell lookup is not implemented yet — pass icon.texture to a Texture2D "
							 "(or import cells as individual textures)."),
						TEXT("ICON_NOT_FOUND"),
						Next);
				}
			}
		}
	}
	if (TexturePath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("set_slot_icon requires specification.icon.texture."));
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Dry run: would set %s icon on %s."), *SlotName, *AssetPath);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	bool bCreated = false;
	UWidgetBlueprint* WBP = FUeremcpUIService::LoadOrCreateWidgetBlueprint(
		AssetPath, FString(), false, false, Err, bCreated);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	const FString IconWidgetName = SlotName + TEXT("_Icon");
	UWidget* Found = WBP->WidgetTree->FindWidget(FName(*IconWidgetName));
	if (!Found)
	{
		Found = WBP->WidgetTree->FindWidget(FName(*SlotName));
	}
	UImage* Image = Cast<UImage>(Found);
	if (!Image)
	{
		// Slot border may own Overlay/Icon — search by suffix.
		WBP->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			if (!Image && W->GetName().Equals(IconWidgetName, ESearchCase::IgnoreCase))
			{
				Image = Cast<UImage>(W);
			}
		});
	}
	if (!Image)
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(TEXT("hint"), TEXT("Slot_<Grid>_rR_cC_Icon or Slot_Hotbar_N_Icon"));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Could not find icon widget for slot '%s'."), *SlotName),
			TEXT("ICON_NOT_FOUND"),
			Next);
	}

	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!Tex)
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(TEXT("specification.icon.texture"), TEXT("/Engine/EngineResources/DefaultTexture"));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Could not load texture '%s'."), *TexturePath),
			TEXT("ICON_NOT_FOUND"),
			Next);
	}
	Image->SetBrushFromTexture(Tex);

	int32 StackCount = 0;
	bool bShowStack = false;
	const TSharedPtr<FJsonObject>* Stack = nullptr;
	if (Request.Specification->TryGetObjectField(TEXT("stack"), Stack) && Stack)
	{
		(*Stack)->TryGetNumberField(TEXT("count"), StackCount);
		(*Stack)->TryGetBoolField(TEXT("show"), bShowStack);
	}
	if (bShowStack)
	{
		if (UTextBlock* StackText = Cast<UTextBlock>(
			WBP->WidgetTree->FindWidget(FName(*(SlotName + TEXT("_Stack"))))))
		{
			StackText->SetText(FText::AsNumber(StackCount));
		}
	}

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
	Response.Summary = FString::Printf(TEXT("Set icon on %s from %s."), *SlotName, *TexturePath);
	Response.Metrics.McpRoundTrips = 1;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::SetContainerWeight(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("set_container_weight"), Request, Rejection))
	{
		return Rejection;
	}
	const FString AssetPath = SpecPath(Request);
	if (AssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("set_container_weight requires widget_blueprint."));
	}
	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = TEXT("Dry run: would update container weight labels.");
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	bool bCreated = false;
	UWidgetBlueprint* WBP = FUeremcpUIService::LoadOrCreateWidgetBlueprint(
		AssetPath, FString(), false, false, Err, bCreated);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	int32 Updated = 0;
	TArray<FString> Missing;
	auto ApplyOne = [&](const TSharedPtr<FJsonObject>& Obj)
	{
		if (!Obj.IsValid())
		{
			return;
		}
		FString LabelWidget;
		double Current = 0.0;
		double MaxKg = 0.0;
		Obj->TryGetStringField(TEXT("label_widget"), LabelWidget);
		Obj->TryGetNumberField(TEXT("current_kg"), Current);
		Obj->TryGetNumberField(TEXT("max_kg"), MaxKg);
		if (LabelWidget.IsEmpty())
		{
			FString Id;
			Obj->TryGetStringField(TEXT("id"), Id);
			if (!Id.IsEmpty())
			{
				LabelWidget = FString::Printf(TEXT("Txt_%sWeight"), *CapitalizeId(Id));
			}
		}
		UTextBlock* Text = Cast<UTextBlock>(WBP->WidgetTree->FindWidget(FName(*LabelWidget)));
		if (!Text)
		{
			Missing.Add(LabelWidget);
			return;
		}
		Text->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.0f KG"), Current, MaxKg)));
		if (Current > MaxKg)
		{
			Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.25f, 0.2f, 1.f)));
		}
		++Updated;
	};

	const TArray<TSharedPtr<FJsonValue>>* Containers = nullptr;
	if (Request.Specification->TryGetArrayField(TEXT("containers"), Containers) && Containers)
	{
		for (const TSharedPtr<FJsonValue>& V : *Containers)
		{
			ApplyOne(V->AsObject());
		}
	}
	const TSharedPtr<FJsonObject>* Total = nullptr;
	if (Request.Specification->TryGetObjectField(TEXT("total"), Total) && Total)
	{
		ApplyOne(*Total);
	}

	if (Updated == 0)
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> MissJson;
		for (const FString& M : Missing)
		{
			MissJson.Add(MakeShared<FJsonValueString>(M));
		}
		Next->SetArrayField(TEXT("missing_label_widgets"), MissJson);
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("No container weight labels updated."),
			TEXT("UNKNOWN"),
			Next);
	}

	const bool bSave = OptionsFlag(Request, TEXT("save"), true);
	if (!FUeremcpUIService::CompileAndOptionallySave(WBP, true, bSave, Err))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = Missing.Num() > 0 ? TEXT("partially_completed") : TEXT("modified");
	Response.PrimaryAsset = AssetPath;
	Response.Summary = FString::Printf(TEXT("Updated %d container weight label(s)."), Updated);
	Response.Metrics.McpRoundTrips = 1;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpUIToolset::SetSlotLayout(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("set_slot_layout"), Request, Rejection))
	{
		return Rejection;
	}
	const FString AssetPath = SpecPath(Request);
	FString WidgetName;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("widget"), WidgetName);
	}
	if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("set_slot_layout requires widget_blueprint and widget."));
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = TEXT("Dry run: would apply slot layout.");
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString Err;
	bool bCreated = false;
	UWidgetBlueprint* WBP = FUeremcpUIService::LoadOrCreateWidgetBlueprint(
		AssetPath, FString(), false, false, Err, bCreated);
	if (!WBP)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}
	UWidget* Widget = WBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Widget '%s' not found."), *WidgetName));
	}

	TArray<FString> Notes;
	const TSharedPtr<FJsonObject>* Canvas = nullptr;
	if (Request.Specification->TryGetObjectField(TEXT("canvas"), Canvas) && Canvas)
	{
		if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			FString Anchors = TEXT("fill");
			(*Canvas)->TryGetStringField(TEXT("anchors"), Anchors);
			if (Anchors.Equals(TEXT("fill"), ESearchCase::IgnoreCase))
			{
				CSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			}
			const TArray<TSharedPtr<FJsonValue>>* Off = nullptr;
			if ((*Canvas)->TryGetArrayField(TEXT("offsets"), Off) && Off && Off->Num() >= 4)
			{
				CSlot->SetOffsets(FMargin(
					(*Off)[0]->AsNumber(), (*Off)[1]->AsNumber(),
					(*Off)[2]->AsNumber(), (*Off)[3]->AsNumber()));
			}
			else
			{
				CSlot->SetOffsets(FMargin(0.f));
			}
		}
		else
		{
			Notes.Add(TEXT("widget has no CanvasPanelSlot — canvas layout skipped"));
		}
	}
	const TSharedPtr<FJsonObject>* Box = nullptr;
	if (Request.Specification->TryGetObjectField(TEXT("box"), Box) && Box)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pad = nullptr;
		if ((*Box)->TryGetArrayField(TEXT("padding"), Pad) && Pad && Pad->Num() >= 4)
		{
			if (UBorder* Border = Cast<UBorder>(Widget))
			{
				Border->SetPadding(FMargin(
					(*Pad)[0]->AsNumber(), (*Pad)[1]->AsNumber(),
					(*Pad)[2]->AsNumber(), (*Pad)[3]->AsNumber()));
			}
			else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
			{
				VSlot->SetPadding(FMargin(
					(*Pad)[0]->AsNumber(), (*Pad)[1]->AsNumber(),
					(*Pad)[2]->AsNumber(), (*Pad)[3]->AsNumber()));
			}
			else
			{
				Notes.Add(TEXT("padding applied only for Border or VerticalBoxSlot — soft-skipped"));
			}
		}
		if ((*Box)->HasField(TEXT("size")))
		{
			Notes.Add(TEXT("box.size is invalid on VerticalBoxSlot — ignored (suggested: size_rule)"));
		}
	}

	const bool bSave = OptionsFlag(Request, TEXT("save"), true);
	if (!FUeremcpUIService::CompileAndOptionallySave(WBP, true, bSave, Err))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Err);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = Notes.Num() > 0 ? TEXT("partially_completed") : TEXT("modified");
	Response.PrimaryAsset = AssetPath;
	Response.Summary = FString::Printf(TEXT("Applied slot layout to %s."), *WidgetName);
	Response.InterpretationNotes = Notes;
	Response.Metrics.McpRoundTrips = 1;
	return FUeremcpEnvelope::SerializeResponse(Response);
}
