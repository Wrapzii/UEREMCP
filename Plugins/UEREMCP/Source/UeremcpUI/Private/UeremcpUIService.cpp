// UEREMCP — UI theme + widget-tree helpers.

#include "UeremcpUIService.h"

#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
	FSlateBrush MakeSolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		Brush.Margin = FMargin(0.25f);
		return Brush;
	}

	FLinearColor ReadColor(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* Key,
		const FLinearColor& Fallback)
	{
		if (!Obj.IsValid())
		{
			return Fallback;
		}
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Key, Arr))
		{
			FLinearColor Out = Fallback;
			if (FUeremcpUIService::ParseColorArray(Arr, Out))
			{
				return Out;
			}
		}
		return Fallback;
	}
}

bool FUeremcpUIService::ParseColorArray(const TArray<TSharedPtr<FJsonValue>>* Arr, FLinearColor& Out)
{
	if (!Arr || Arr->Num() < 3)
	{
		return false;
	}
	Out.R = static_cast<float>((*Arr)[0]->AsNumber());
	Out.G = static_cast<float>((*Arr)[1]->AsNumber());
	Out.B = static_cast<float>((*Arr)[2]->AsNumber());
	Out.A = Arr->Num() >= 4 ? static_cast<float>((*Arr)[3]->AsNumber()) : 1.f;
	return true;
}

FUeremcpUITheme FUeremcpUIService::ThemeFromId(const FString& ThemeId)
{
	FUeremcpUITheme Theme;
	Theme.Id = ThemeId.IsEmpty() ? TEXT("northridge_fantasy") : ThemeId;
	if (Theme.Id.Equals(TEXT("northridge_diegetic"), ESearchCase::IgnoreCase))
	{
		Theme.PanelColor = FLinearColor(0.04f, 0.035f, 0.03f, 0.94f);
		Theme.TextColor = FLinearColor(0.9f, 0.82f, 0.65f, 1.f);
		Theme.TitleSize = 36;
		Theme.BodySize = 16;
		Theme.ButtonNormal = FLinearColor(0.1f, 0.08f, 0.06f, 1.f);
		Theme.ButtonHovered = FLinearColor(0.28f, 0.16f, 0.08f, 1.f);
		Theme.Accent = FLinearColor(0.65f, 0.42f, 0.18f, 1.f);
		Theme.ButtonOutline = Theme.Accent;
	}
	return Theme;
}

FUeremcpUITheme FUeremcpUIService::ThemeFromJson(
	const TSharedPtr<FJsonObject>& ThemeObj,
	const FString& FallbackId)
{
	FUeremcpUITheme Theme = ThemeFromId(FallbackId);
	if (!ThemeObj.IsValid())
	{
		return Theme;
	}
	ThemeObj->TryGetStringField(TEXT("id"), Theme.Id);
	const TSharedPtr<FJsonObject>* Panel = nullptr;
	if (ThemeObj->TryGetObjectField(TEXT("panel"), Panel) && Panel)
	{
		Theme.PanelColor = ReadColor(*Panel, TEXT("color"), Theme.PanelColor);
		(*Panel)->TryGetNumberField(TEXT("padding"), Theme.PanelPadding);
	}
	const TSharedPtr<FJsonObject>* Text = nullptr;
	if (ThemeObj->TryGetObjectField(TEXT("text"), Text) && Text)
	{
		Theme.TextColor = ReadColor(*Text, TEXT("color"), Theme.TextColor);
		double Size = Theme.TitleSize;
		if ((*Text)->TryGetNumberField(TEXT("title_size"), Size))
		{
			Theme.TitleSize = static_cast<int32>(Size);
		}
		Size = Theme.BodySize;
		if ((*Text)->TryGetNumberField(TEXT("body_size"), Size))
		{
			Theme.BodySize = static_cast<int32>(Size);
		}
		Size = Theme.LetterSpacing;
		if ((*Text)->TryGetNumberField(TEXT("letter_spacing"), Size))
		{
			Theme.LetterSpacing = static_cast<int32>(Size);
		}
	}
	const TSharedPtr<FJsonObject>* Button = nullptr;
	if (ThemeObj->TryGetObjectField(TEXT("button"), Button) && Button)
	{
		Theme.ButtonNormal = ReadColor(*Button, TEXT("normal"), Theme.ButtonNormal);
		Theme.ButtonHovered = ReadColor(*Button, TEXT("hovered"), Theme.ButtonHovered);
		Theme.ButtonOutline = ReadColor(*Button, TEXT("outline"), Theme.ButtonOutline);
	}
	return Theme;
}

bool FUeremcpUIService::GetSpecString(
	const TSharedPtr<FJsonObject>& Spec,
	const TCHAR* Key,
	FString& OutValue)
{
	return Spec.IsValid() && Spec->TryGetStringField(Key, OutValue) && !OutValue.IsEmpty();
}

UWidgetBlueprint* FUeremcpUIService::LoadOrCreateWidgetBlueprint(
	const FString& AssetPath,
	const FString& ParentClassPath,
	bool bCreateIfMissing,
	bool bDryRun,
	FString& OutError,
	bool& bOutCreated)
{
	bOutCreated = false;
	OutError.Reset();

	FString SoftPath = AssetPath;
	if (!SoftPath.Contains(TEXT(".")))
	{
		SoftPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	}

	if (UObject* Existing = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *SoftPath))
	{
		return Cast<UWidgetBlueprint>(Existing);
	}
	if (UObject* ExistingPkg = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath))
	{
		return Cast<UWidgetBlueprint>(ExistingPkg);
	}

	if (!bCreateIfMissing)
	{
		OutError = FString::Printf(TEXT("Widget Blueprint not found: %s"), *AssetPath);
		return nullptr;
	}
	if (bDryRun)
	{
		return nullptr;
	}

	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetShortName(AssetPath);
	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		OutError = TEXT("asset_path must look like /Game/UI/Folder/WBP_Name");
		return nullptr;
	}

	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	UClass* ParentClass = UUserWidget::StaticClass();
	if (!ParentClassPath.IsEmpty())
	{
		if (UClass* Loaded = LoadClass<UUserWidget>(nullptr, *ParentClassPath))
		{
			ParentClass = Loaded;
		}
		else if (UObject* Obj = StaticLoadObject(UClass::StaticClass(), nullptr, *ParentClassPath))
		{
			if (UClass* AsClass = Cast<UClass>(Obj))
			{
				ParentClass = AsClass;
			}
		}
	}
	Factory->ParentClass = ParentClass;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* Created = AssetTools.CreateAsset(
		AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
	UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(Created);
	if (!WBP)
	{
		OutError = FString::Printf(TEXT("Failed to create Widget Blueprint at %s"), *AssetPath);
		return nullptr;
	}
	bOutCreated = true;
	return WBP;
}

bool FUeremcpUIService::CompileAndOptionallySave(
	UWidgetBlueprint* WBP,
	bool bCompile,
	bool bSave,
	FString& OutError)
{
	if (!WBP)
	{
		OutError = TEXT("null Widget Blueprint");
		return false;
	}
	if (bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(WBP);
	}
	if (bSave)
	{
		UPackage* Package = WBP->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, WBP, *Filename, Args))
		{
			OutError = FString::Printf(TEXT("SavePackage failed for %s"), *Package->GetName());
			return false;
		}
	}
	return true;
}

void FUeremcpUIService::ApplyTextStyle(UTextBlock* Text, const FUeremcpUITheme& Theme, bool bTitle)
{
	if (!Text)
	{
		return;
	}
	Text->SetColorAndOpacity(FSlateColor(Theme.TextColor));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = bTitle ? Theme.TitleSize : Theme.BodySize;
	Font.LetterSpacing = Theme.LetterSpacing;
	Text->SetFont(Font);
}

void FUeremcpUIService::ApplyButtonStyle(UButton* Button, const FUeremcpUITheme& Theme)
{
	if (!Button)
	{
		return;
	}
	FButtonStyle Style = Button->GetStyle();
	Style.Normal = MakeSolidBrush(Theme.ButtonNormal);
	Style.Hovered = MakeSolidBrush(Theme.ButtonHovered);
	Style.Pressed = MakeSolidBrush(Theme.ButtonHovered * 0.85f);
	Style.Disabled = MakeSolidBrush(Theme.ButtonNormal * 0.5f);
	Style.NormalPadding = FMargin(12.f, 8.f);
	Style.PressedPadding = FMargin(12.f, 8.f);
	Button->SetStyle(Style);
}

void FUeremcpUIService::ApplyPanelBorder(UBorder* Border, const FUeremcpUITheme& Theme)
{
	if (!Border)
	{
		return;
	}
	Border->SetBrushColor(Theme.PanelColor);
	Border->SetPadding(FMargin(Theme.PanelPadding));
}

void FUeremcpUIService::ApplyThemeToTree(
	UWidgetBlueprint* WBP,
	const FUeremcpUITheme& Theme,
	int32& OutStyled)
{
	OutStyled = 0;
	if (!WBP || !WBP->WidgetTree)
	{
		return;
	}
	WBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			const bool bTitle = Widget->GetFName().ToString().Contains(TEXT("Title"))
				|| Widget->GetFName().ToString().Contains(TEXT("Brand"));
			ApplyTextStyle(Text, Theme, bTitle);
			++OutStyled;
		}
		else if (UButton* Button = Cast<UButton>(Widget))
		{
			ApplyButtonStyle(Button, Theme);
			++OutStyled;
		}
		else if (UBorder* Border = Cast<UBorder>(Widget))
		{
			ApplyPanelBorder(Border, Theme);
			++OutStyled;
		}
	});
}

UWidget* FUeremcpUIService::BuildOverlayLeftPanel(
	UWidgetTree* Tree,
	UPanelWidget* Parent,
	const FString& ScreenId,
	const TSharedPtr<FJsonObject>& Screen,
	const FUeremcpUITheme& Theme,
	TArray<FString>& OutWidgetNames)
{
	if (!Tree || !Parent)
	{
		return nullptr;
	}

	const FString Prefix = FString::Printf(TEXT("Scr_%s_"), *ScreenId);
	UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*(Prefix + TEXT("Panel"))));
	ApplyPanelBorder(Panel, Theme);
	Parent->AddChild(Panel);
	OutWidgetNames.Add(Panel->GetName());

	UVerticalBox* VBox = Tree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), FName(*(Prefix + TEXT("VBox"))));
	Panel->SetContent(VBox);
	OutWidgetNames.Add(VBox->GetName());

	FString Title = TEXT("NORTHRIDGE ONLINE");
	FString Tagline;
	const TSharedPtr<FJsonObject>* Brand = nullptr;
	if (Screen.IsValid() && Screen->TryGetObjectField(TEXT("brand"), Brand) && Brand)
	{
		(*Brand)->TryGetStringField(TEXT("title"), Title);
		(*Brand)->TryGetStringField(TEXT("tagline"), Tagline);
	}

	UTextBlock* TitleText = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*(Prefix + TEXT("BrandTitle"))));
	TitleText->SetText(FText::FromString(Title));
	ApplyTextStyle(TitleText, Theme, true);
	if (UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(TitleText))
	{
		Slot->SetPadding(FMargin(0, 0, 0, 8));
	}
	OutWidgetNames.Add(TitleText->GetName());

	if (!Tagline.IsEmpty())
	{
		UTextBlock* Tag = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Prefix + TEXT("BrandTagline"))));
		Tag->SetText(FText::FromString(Tagline));
		ApplyTextStyle(Tag, Theme, false);
		VBox->AddChildToVerticalBox(Tag);
		OutWidgetNames.Add(Tag->GetName());
	}

	const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
	if (Screen.IsValid() && Screen->TryGetArrayField(TEXT("fields"), Fields) && Fields)
	{
		for (const TSharedPtr<FJsonValue>& FieldVal : *Fields)
		{
			const FString FieldName = FieldVal->AsString();
			if (FieldName.IsEmpty())
			{
				continue;
			}
			UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("%sField_%s"), *Prefix, *FieldName)));
			Label->SetText(FText::FromString(FieldName.Replace(TEXT("_"), TEXT(" ")).ToUpper()));
			ApplyTextStyle(Label, Theme, false);
			VBox->AddChildToVerticalBox(Label);
			OutWidgetNames.Add(Label->GetName());
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
	if (Screen.IsValid() && Screen->TryGetArrayField(TEXT("actions"), Actions) && Actions)
	{
		for (const TSharedPtr<FJsonValue>& ActionVal : *Actions)
		{
			const FString ActionName = ActionVal->AsString();
			if (ActionName.IsEmpty())
			{
				continue;
			}
			const FString Safe = ActionName.Replace(TEXT(" "), TEXT("_"));
			UButton* Btn = Tree->ConstructWidget<UButton>(
				UButton::StaticClass(),
				FName(*FString::Printf(TEXT("%sBtn_%s"), *Prefix, *Safe)));
			ApplyButtonStyle(Btn, Theme);
			UTextBlock* BtnText = Tree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("%sBtnTxt_%s"), *Prefix, *Safe)));
			BtnText->SetText(FText::FromString(ActionName));
			ApplyTextStyle(BtnText, Theme, false);
			Btn->AddChild(BtnText);
			if (UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(Btn))
			{
				Slot->SetPadding(FMargin(0, 6, 0, 0));
			}
			OutWidgetNames.Add(Btn->GetName());
			OutWidgetNames.Add(BtnText->GetName());
		}
	}

	return Panel;
}

UWidget* FUeremcpUIService::BuildSlotCell(
	UWidgetTree* Tree,
	UPanelWidget* Parent,
	const FString& SlotName,
	int32 CellPx,
	const FUeremcpUITheme& Theme,
	const FString& IndexLabel,
	int32 GridRow,
	int32 GridCol)
{
	if (!Tree || !Parent)
	{
		return nullptr;
	}

	USizeBox* Size = Tree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName(*(SlotName + TEXT("_Size"))));
	Size->SetWidthOverride(static_cast<float>(CellPx));
	Size->SetHeightOverride(static_cast<float>(CellPx));

	UBorder* Cell = Tree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName(*SlotName));
	Cell->SetBrushColor(Theme.SlotEmpty);
	Cell->SetPadding(FMargin(4.f));

	UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), FName(*(SlotName + TEXT("_Overlay"))));
	UImage* Icon = Tree->ConstructWidget<UImage>(
		UImage::StaticClass(), FName(*(SlotName + TEXT("_Icon"))));
	Icon->SetColorAndOpacity(FLinearColor(0.2f, 0.18f, 0.15f, 1.f));
	Overlay->AddChildToOverlay(Icon);

	if (!IndexLabel.IsEmpty())
	{
		UTextBlock* Idx = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(SlotName + TEXT("_Idx"))));
		Idx->SetText(FText::FromString(IndexLabel));
		ApplyTextStyle(Idx, Theme, false);
		if (UOverlaySlot* OSlot = Overlay->AddChildToOverlay(Idx))
		{
			OSlot->SetHorizontalAlignment(HAlign_Left);
			OSlot->SetVerticalAlignment(VAlign_Top);
		}
	}

	UTextBlock* Stack = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*(SlotName + TEXT("_Stack"))));
	Stack->SetText(FText::GetEmpty());
	ApplyTextStyle(Stack, Theme, false);
	if (UOverlaySlot* OSlot = Overlay->AddChildToOverlay(Stack))
	{
		OSlot->SetHorizontalAlignment(HAlign_Right);
		OSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	Cell->SetContent(Overlay);
	Size->SetContent(Cell);

	if (UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Parent))
	{
		const int32 Row = GridRow == INDEX_NONE ? 0 : GridRow;
		const int32 Col = GridCol == INDEX_NONE ? 0 : GridCol;
		Grid->AddChildToUniformGrid(Size, Row, Col);
	}
	else if (UHorizontalBox* HBox = Cast<UHorizontalBox>(Parent))
	{
		if (UHorizontalBoxSlot* HSlot = HBox->AddChildToHorizontalBox(Size))
		{
			HSlot->SetPadding(FMargin(2.f));
		}
	}
	else if (UVerticalBox* VBox = Cast<UVerticalBox>(Parent))
	{
		if (UVerticalBoxSlot* VSlot = VBox->AddChildToVerticalBox(Size))
		{
			VSlot->SetPadding(FMargin(2.f));
		}
	}
	else
	{
		Parent->AddChild(Size);
	}
	return Cell;
}
