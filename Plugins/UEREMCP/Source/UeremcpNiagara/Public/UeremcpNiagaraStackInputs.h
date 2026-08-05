// UEREMCP — stack input apply helpers (local / linked / DI / HLSL / enum / dynamic).
//
// [VERIFIED: FNiagaraExt_StackInputData_* + SetStackInputData —
//  NiagaraExternalSystemEditorUtilities.h:520-594, 1377]
// [VERIFIED: InitStackInputFromValue Linked/DI/HLSL/Dynamic —
//  NiagaraExternalSystemEditorUtilities.cpp:2831-2910]

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NiagaraExternalSystemEditorUtilities.h"

/** Apply module inputs{} JSON beyond number|bool|[rgba] locals. */
class FUeremcpNiagaraStackInputs
{
public:
	/**
	 * Convert one JSON value into FNiagaraExt_StackInputValue for SetStackInputData.
	 * Supported shapes:
	 *   number | bool | [r,g,b,a]  → local literals
	 *   { "mode":"linked", "linked_variable":"User.X" } or { "linked":"User.X" }
	 *   { "mode":"hlsl_expression", "hlsl_expression":"..." }
	 *   { "mode":"data_interface", "data_interface"|property_values:{...} }
	 *   { "mode":"dynamic_input", "script"|"asset_path":"/Niagara/..." }
	 *   { "mode":"enum", "enum_value":"Enum::Name" }
	 *   { "mode":"local", "value": number|bool|[rgba] }
	 * ExistingValue supplies type hints for local overwrite / enum Enum ptr.
	 */
	static bool TryBuildStackInputValue(
		const FNiagaraExt_StackInputValue& ExistingValue,
		const TSharedPtr<FJsonValue>& JsonVal,
		FNiagaraExt_StackInputValue& OutValue,
		FString& OutSkipReason);

	/** Apply Inputs object on a module via GetModuleInputValues + SetStackInputData. */
	static void ApplyModuleInputs(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		const FString& ScriptUsage,
		const FName& ModuleName,
		const TSharedPtr<FJsonObject>& Inputs,
		int32& InOutOps,
		TArray<FString>& OutWarnings,
		TArray<FString>& OutApplied);
};
