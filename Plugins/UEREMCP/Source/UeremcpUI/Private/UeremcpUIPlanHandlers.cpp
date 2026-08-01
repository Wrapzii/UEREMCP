#include "UeremcpUIPlanHandlers.h"

#include "UeremcpUIToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FUIToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const FString& Action,
		FUIToolFn Fn,
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = Fn(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s returned an empty response"), *Action);
			return false;
		}
		return true;
	}

	const TArray<FString>& ActionNames()
	{
		static const TArray<FString> Names = {
			TEXT("create_widget_from_spec"),
			TEXT("apply_ui_theme"),
			TEXT("show_widget_in_world"),
			TEXT("show_widget_on_screen"),
			TEXT("capture_ui_frame"),
			TEXT("spawn_character_preview"),
			TEXT("save_widget_asset"),
			TEXT("create_inventory_sheet_from_spec"),
			TEXT("set_slot_icon"),
			TEXT("set_container_weight"),
			TEXT("set_slot_layout"),
		};
		return Names;
	}
}

bool FUeremcpUIPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&](const FString& Action, FUIToolFn Fn) -> bool
	{
		FString LocalError;
		const bool bOk = FUeremcpPlanExecutor::RegisterAction(
			Action,
			[Action, Fn](const FString& RequestJson, FString& OutResponseJson, FString& Err) -> bool
			{
				return DispatchTool(Action, Fn, RequestJson, OutResponseJson, Err);
			},
			LocalError);
		if (!bOk)
		{
			OutError = LocalError;
		}
		return bOk;
	};

	if (!Bind(TEXT("create_widget_from_spec"), &UUeremcpUIToolset::CreateWidgetFromSpec)) return false;
	if (!Bind(TEXT("apply_ui_theme"), &UUeremcpUIToolset::ApplyUiTheme)) return false;
	if (!Bind(TEXT("show_widget_in_world"), &UUeremcpUIToolset::ShowWidgetInWorld)) return false;
	if (!Bind(TEXT("show_widget_on_screen"), &UUeremcpUIToolset::ShowWidgetOnScreen)) return false;
	if (!Bind(TEXT("capture_ui_frame"), &UUeremcpUIToolset::CaptureUiFrame)) return false;
	if (!Bind(TEXT("spawn_character_preview"), &UUeremcpUIToolset::SpawnCharacterPreview)) return false;
	if (!Bind(TEXT("save_widget_asset"), &UUeremcpUIToolset::SaveWidgetAsset)) return false;
	if (!Bind(TEXT("create_inventory_sheet_from_spec"), &UUeremcpUIToolset::CreateInventorySheetFromSpec)) return false;
	if (!Bind(TEXT("set_slot_icon"), &UUeremcpUIToolset::SetSlotIcon)) return false;
	if (!Bind(TEXT("set_container_weight"), &UUeremcpUIToolset::SetContainerWeight)) return false;
	if (!Bind(TEXT("set_slot_layout"), &UUeremcpUIToolset::SetSlotLayout)) return false;
	OutError.Reset();
	return true;
}

void FUeremcpUIPlanHandlers::Unregister()
{
	for (const FString& Name : ActionNames())
	{
		FUeremcpPlanExecutor::UnregisterAction(Name);
	}
}
