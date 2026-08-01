#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UWidgetBlueprint;
class UWidget;
class UPanelWidget;
class UTextBlock;
class UButton;
class UBorder;
class UWidgetTree;

struct FUeremcpUITheme
{
	FString Id = TEXT("northridge_fantasy");
	FLinearColor PanelColor = FLinearColor(0.06f, 0.05f, 0.04f, 0.92f);
	float PanelPadding = 24.f;
	FLinearColor TextColor = FLinearColor(0.92f, 0.86f, 0.72f, 1.f);
	int32 TitleSize = 48;
	int32 BodySize = 18;
	int32 LetterSpacing = 8;
	FLinearColor ButtonNormal = FLinearColor(0.12f, 0.1f, 0.08f, 1.f);
	FLinearColor ButtonHovered = FLinearColor(0.35f, 0.22f, 0.1f, 1.f);
	FLinearColor ButtonOutline = FLinearColor(0.72f, 0.5f, 0.22f, 1.f);
	FLinearColor SlotEmpty = FLinearColor(0.08f, 0.07f, 0.06f, 0.85f);
	FLinearColor Accent = FLinearColor(0.72f, 0.5f, 0.22f, 1.f);
};

struct FUeremcpUIService
{
	static FUeremcpUITheme ThemeFromId(const FString& ThemeId);
	static FUeremcpUITheme ThemeFromJson(const TSharedPtr<FJsonObject>& ThemeObj, const FString& FallbackId);
	static bool ParseColorArray(const TArray<TSharedPtr<FJsonValue>>* Arr, FLinearColor& Out);

	static bool GetSpecString(
		const TSharedPtr<FJsonObject>& Spec,
		const TCHAR* Key,
		FString& OutValue);

	static UWidgetBlueprint* LoadOrCreateWidgetBlueprint(
		const FString& AssetPath,
		const FString& ParentClassPath,
		bool bCreateIfMissing,
		bool bDryRun,
		FString& OutError,
		bool& bOutCreated);

	static bool CompileAndOptionallySave(
		UWidgetBlueprint* WBP,
		bool bCompile,
		bool bSave,
		FString& OutError);

	static void ApplyTextStyle(UTextBlock* Text, const FUeremcpUITheme& Theme, bool bTitle);
	static void ApplyButtonStyle(UButton* Button, const FUeremcpUITheme& Theme);
	static void ApplyPanelBorder(UBorder* Border, const FUeremcpUITheme& Theme);
	static void ApplyThemeToTree(UWidgetBlueprint* WBP, const FUeremcpUITheme& Theme, int32& OutStyled);

	static UWidget* BuildOverlayLeftPanel(
		UWidgetTree* Tree,
		UPanelWidget* Parent,
		const FString& ScreenId,
		const TSharedPtr<FJsonObject>& Screen,
		const FUeremcpUITheme& Theme,
		TArray<FString>& OutWidgetNames);

	static UWidget* BuildSlotCell(
		UWidgetTree* Tree,
		UPanelWidget* Parent,
		const FString& SlotName,
		int32 CellPx,
		const FUeremcpUITheme& Theme,
		const FString& IndexLabel = FString(),
		int32 GridRow = INDEX_NONE,
		int32 GridCol = INDEX_NONE);
};
