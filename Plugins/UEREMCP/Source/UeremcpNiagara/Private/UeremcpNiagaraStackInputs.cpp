// UEREMCP — stack input apply helpers (WS-07).

#include "UeremcpNiagaraStackInputs.h"

#include "NiagaraScript.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	FString ContextErrorsToString(const FNiagaraExternalEditContext& Context)
	{
		TArray<FString> Lines;
		for (const FText& Error : Context.Errors)
		{
			Lines.Add(Error.ToString());
		}
		return FString::Join(Lines, TEXT("; "));
	}

	bool TryBuildLocalFromPrimitive(
		const FNiagaraExt_StackInputValue& ExistingValue,
		const TSharedPtr<FJsonValue>& JsonVal,
		FNiagaraExt_StackInputValue& OutValue,
		FString& OutSkipReason)
	{
		if (!JsonVal.IsValid())
		{
			OutSkipReason = TEXT("null input value");
			return false;
		}

		if (JsonVal->Type == EJson::Number)
		{
			FNiagaraExt_StackInputValue Scratch = ExistingValue;
			if (FNiagaraFloat* AsFloat = Scratch.GetMutablePtr<FNiagaraFloat>())
			{
				AsFloat->Value = static_cast<float>(JsonVal->AsNumber());
				OutValue = Scratch;
				return true;
			}
			if (FNiagaraInt32* AsInt = Scratch.GetMutablePtr<FNiagaraInt32>())
			{
				AsInt->Value = static_cast<int32>(JsonVal->AsNumber());
				OutValue = Scratch;
				return true;
			}
			FNiagaraFloat& Fresh = OutValue.InitializeAs<FNiagaraFloat>();
			Fresh.Value = static_cast<float>(JsonVal->AsNumber());
			return true;
		}

		if (JsonVal->Type == EJson::Boolean)
		{
			FNiagaraBool& AsBool = OutValue.InitializeAs<FNiagaraBool>();
			AsBool.SetValue(JsonVal->AsBool());
			return true;
		}

		if (JsonVal->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Arr = JsonVal->AsArray();
			if (Arr.Num() >= 3)
			{
				FLinearColor& Color = OutValue.InitializeAs<FLinearColor>();
				Color.R = static_cast<float>(Arr[0]->AsNumber());
				Color.G = static_cast<float>(Arr[1]->AsNumber());
				Color.B = static_cast<float>(Arr[2]->AsNumber());
				Color.A = Arr.Num() > 3 ? static_cast<float>(Arr[3]->AsNumber()) : 1.0f;
				return true;
			}
			OutSkipReason = TEXT("array local requires [r,g,b] or [r,g,b,a]");
			return false;
		}

		OutSkipReason = TEXT("unsupported local JSON shape");
		return false;
	}

	bool TryBuildFromObject(
		const FNiagaraExt_StackInputValue& ExistingValue,
		const TSharedPtr<FJsonObject>& Obj,
		FNiagaraExt_StackInputValue& OutValue,
		FString& OutSkipReason)
	{
		FString Mode;
		Obj->TryGetStringField(TEXT("mode"), Mode);
		Mode = Mode.ToLower();

		FString LinkedName;
		if (Obj->TryGetStringField(TEXT("linked"), LinkedName)
			|| Obj->TryGetStringField(TEXT("linked_variable"), LinkedName)
			|| (Mode == TEXT("linked") && Obj->TryGetStringField(TEXT("variable"), LinkedName)))
		{
			if (LinkedName.IsEmpty())
			{
				OutSkipReason = TEXT("linked input requires linked_variable / linked name");
				return false;
			}
			// [VERIFIED: FNiagaraExt_StackInputData_Linked —
			//  NiagaraExternalSystemEditorUtilities.h:555-561]
			// [VERIFIED: SetLinkedParameterValue path —
			//  NiagaraExternalSystemEditorUtilities.cpp:2854-2856]
			FNiagaraExt_StackInputData_Linked& Linked =
				OutValue.InitializeAs<FNiagaraExt_StackInputData_Linked>();
			Linked.LinkedVariable.Name = FName(*LinkedName);
			if (const FNiagaraExt_StackInputData_Linked* ExistingLinked =
				ExistingValue.GetPtr<FNiagaraExt_StackInputData_Linked>())
			{
				Linked.LinkedVariable.Type = ExistingLinked->LinkedVariable.Type;
			}
			return true;
		}

		if (Mode == TEXT("hlsl_expression") || Mode == TEXT("hlsl") || Mode == TEXT("expression")
			|| Obj->HasField(TEXT("hlsl_expression")))
		{
			FString Expr;
			Obj->TryGetStringField(TEXT("hlsl_expression"), Expr);
			if (Expr.IsEmpty())
			{
				Obj->TryGetStringField(TEXT("expression"), Expr);
			}
			if (Expr.IsEmpty())
			{
				OutSkipReason = TEXT("hlsl_expression requires hlsl_expression string");
				return false;
			}
			// [VERIFIED: FNiagaraExt_StackInputData_HlslExpression —
			//  NiagaraExternalSystemEditorUtilities.h:564-571]
			FNiagaraExt_StackInputData_HlslExpression& Hlsl =
				OutValue.InitializeAs<FNiagaraExt_StackInputData_HlslExpression>();
			Hlsl.HlslExpression = Expr;
			return true;
		}

		if (Mode == TEXT("data_interface") || Mode == TEXT("di")
			|| Obj->HasField(TEXT("data_interface")) || Obj->HasField(TEXT("property_values")))
		{
			FString PropertyJson;
			const TSharedPtr<FJsonObject>* DiObj = nullptr;
			if (Obj->TryGetObjectField(TEXT("data_interface"), DiObj) && DiObj && (*DiObj).IsValid())
			{
				const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropertyJson);
				FJsonSerializer::Serialize((*DiObj).ToSharedRef(), Writer);
			}
			else if (Obj->TryGetObjectField(TEXT("property_values"), DiObj) && DiObj && (*DiObj).IsValid())
			{
				const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PropertyJson);
				FJsonSerializer::Serialize((*DiObj).ToSharedRef(), Writer);
			}
			else
			{
				Obj->TryGetStringField(TEXT("property_values_json"), PropertyJson);
			}
			if (PropertyJson.IsEmpty())
			{
				OutSkipReason = TEXT("data_interface requires data_interface/property_values object");
				return false;
			}
			// [VERIFIED: FNiagaraExt_StackInputData_DataInterface —
			//  NiagaraExternalSystemEditorUtilities.h:574-580]
			FNiagaraExt_StackInputData_DataInterface& DI =
				OutValue.InitializeAs<FNiagaraExt_StackInputData_DataInterface>();
			DI.PropertyValues = PropertyJson;
			return true;
		}

		if (Mode == TEXT("dynamic_input") || Mode == TEXT("dynamic")
			|| ((Obj->HasField(TEXT("script")) || Obj->HasField(TEXT("asset_path")))
				&& Mode == TEXT("dynamic_input")))
		{
			FString ScriptPath;
			Obj->TryGetStringField(TEXT("script"), ScriptPath);
			if (ScriptPath.IsEmpty())
			{
				Obj->TryGetStringField(TEXT("asset_path"), ScriptPath);
			}
			if (ScriptPath.IsEmpty() && Mode != TEXT("dynamic_input"))
			{
				// Fall through — plain script field without mode may be unrelated.
			}
			else
			{
				if (ScriptPath.IsEmpty())
				{
					OutSkipReason = TEXT("dynamic_input requires script / asset_path");
					return false;
				}
				UNiagaraScript* DynScript = Cast<UNiagaraScript>(FSoftObjectPath(ScriptPath).TryLoad());
				if (!DynScript)
				{
					OutSkipReason = FString::Printf(
						TEXT("dynamic_input script not found: %s"), *ScriptPath);
					return false;
				}
				// [VERIFIED: FNiagaraExt_StackInputData_DynamicInput —
				//  NiagaraExternalSystemEditorUtilities.h:587-594]
				FNiagaraExt_StackInputData_DynamicInput& Dyn =
					OutValue.InitializeAs<FNiagaraExt_StackInputData_DynamicInput>();
				Dyn.DynamicInputAsset = DynScript;
				return true;
			}
		}

		if (Mode == TEXT("enum") || Obj->HasField(TEXT("enum_value")))
		{
			FString EnumName;
			Obj->TryGetStringField(TEXT("enum_value"), EnumName);
			if (EnumName.IsEmpty())
			{
				Obj->TryGetStringField(TEXT("value"), EnumName);
			}
			if (EnumName.IsEmpty())
			{
				OutSkipReason = TEXT("enum requires enum_value");
				return false;
			}
			if (const FNiagaraExt_StackInputData_Enum* ExistingEnum =
				ExistingValue.GetPtr<FNiagaraExt_StackInputData_Enum>())
			{
				FNiagaraExt_StackInputData_Enum& EnumVal =
					OutValue.InitializeAs<FNiagaraExt_StackInputData_Enum>();
				EnumVal.Enum = ExistingEnum->Enum;
				EnumVal.EnumName = FName(*EnumName);
				EnumVal.DisplayName = ExistingEnum->DisplayName;
				return true;
			}
			OutSkipReason = TEXT(
				"enum write requires an existing enum-typed stack input (GetModuleInputValues) "
				"so UEnum* is known; no bare enum create via ExternalEditUtilities");
			return false;
		}

		if (Mode == TEXT("local") || Obj->HasField(TEXT("value")))
		{
			// UE 5.8: TryGetField(FieldName) returns TSharedPtr (not out-param).
			// [VERIFIED: Engine/.../JsonObject.h:354]
			const TSharedPtr<FJsonValue> Nested = Obj->TryGetField(TEXT("value"));
			if (Nested.IsValid())
			{
				return TryBuildLocalFromPrimitive(ExistingValue, Nested, OutValue, OutSkipReason);
			}
		}

		// Explicit dynamic_input mode with script path (when mode was set).
		if (Mode == TEXT("dynamic_input"))
		{
			FString ScriptPath;
			Obj->TryGetStringField(TEXT("script"), ScriptPath);
			if (ScriptPath.IsEmpty())
			{
				Obj->TryGetStringField(TEXT("asset_path"), ScriptPath);
			}
			if (ScriptPath.IsEmpty())
			{
				OutSkipReason = TEXT("dynamic_input requires script / asset_path");
				return false;
			}
			UNiagaraScript* DynScript = Cast<UNiagaraScript>(FSoftObjectPath(ScriptPath).TryLoad());
			if (!DynScript)
			{
				OutSkipReason = FString::Printf(
					TEXT("dynamic_input script not found: %s"), *ScriptPath);
				return false;
			}
			FNiagaraExt_StackInputData_DynamicInput& Dyn =
				OutValue.InitializeAs<FNiagaraExt_StackInputData_DynamicInput>();
			Dyn.DynamicInputAsset = DynScript;
			return true;
		}

		OutSkipReason = TEXT(
			"unsupported input object — use number|bool|[rgba] or "
			"{mode:linked|hlsl_expression|data_interface|dynamic_input|enum|local}");
		return false;
	}
}

bool FUeremcpNiagaraStackInputs::TryBuildStackInputValue(
	const FNiagaraExt_StackInputValue& ExistingValue,
	const TSharedPtr<FJsonValue>& JsonVal,
	FNiagaraExt_StackInputValue& OutValue,
	FString& OutSkipReason)
{
	OutSkipReason.Reset();
	OutValue = FNiagaraExt_StackInputValue();

	if (!JsonVal.IsValid() || JsonVal->IsNull())
	{
		OutSkipReason = TEXT("null input value");
		return false;
	}

	if (JsonVal->Type == EJson::Object)
	{
		return TryBuildFromObject(ExistingValue, JsonVal->AsObject(), OutValue, OutSkipReason);
	}

	return TryBuildLocalFromPrimitive(ExistingValue, JsonVal, OutValue, OutSkipReason);
}

void FUeremcpNiagaraStackInputs::ApplyModuleInputs(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	const FString& EmitterName,
	const FString& ScriptUsage,
	const FName& ModuleName,
	const TSharedPtr<FJsonObject>& Inputs,
	int32& InOutOps,
	TArray<FString>& OutWarnings,
	TArray<FString>& OutApplied)
{
	if (!System || !Inputs.IsValid() || Inputs->Values.Num() == 0)
	{
		return;
	}

	FNiagaraExt_StackItemReference ModuleRef(
		System, FName(*EmitterName), FName(*ScriptUsage), ModuleName);
	FNiagaraExt_ModuleInputValues ModuleValues;
	UNiagaraExternalEditUtilities::GetModuleInputValues(ModuleRef, ModuleValues, Context);
	++InOutOps;
	if (Context.HasErrors())
	{
		OutWarnings.Add(ContextErrorsToString(Context));
		Context.Errors.Reset();
		return;
	}

	FNiagaraExt_ModuleTopology ModuleTopo;
	UNiagaraExternalEditUtilities::GetModuleTopology(ModuleRef, ModuleTopo, Context);
	++InOutOps;
	if (Context.HasErrors())
	{
		Context.Errors.Reset();
	}

	TMap<FString, FNiagaraExt_StackInputValue> ByName;
	TMap<FString, FNiagaraTypeDefinition> TypeByName;
	for (const FNiagaraExt_StackInputValueEntry& Entry : ModuleValues.Inputs)
	{
		ByName.Add(Entry.Name.ToString(), Entry.Value);
	}
	for (const FNiagaraExt_StackInputTopology& InputTopo : ModuleTopo.Inputs)
	{
		TypeByName.Add(InputTopo.Name.ToString(), InputTopo.Type);
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Inputs->Values)
	{
		FNiagaraExt_StackInputValue Existing;
		if (const FNiagaraExt_StackInputValue* Found = ByName.Find(Pair.Key))
		{
			Existing = *Found;
		}

		FNiagaraExt_StackInputValue NewValue;
		FString Skip;
		if (!TryBuildStackInputValue(Existing, Pair.Value, NewValue, Skip))
		{
			OutWarnings.Add(FString::Printf(
				TEXT("%s/%s/%s.%s: %s"),
				*EmitterName, *ScriptUsage, *ModuleName.ToString(), *Pair.Key, *Skip));
			continue;
		}

		// Linked writes need a type; copy from module topology when Existing was not already linked.
		if (FNiagaraExt_StackInputData_Linked* Linked =
			NewValue.GetMutablePtr<FNiagaraExt_StackInputData_Linked>())
		{
			if (!Linked->LinkedVariable.Type.IsValid())
			{
				if (const FNiagaraTypeDefinition* FoundType = TypeByName.Find(Pair.Key))
				{
					Linked->LinkedVariable.Type = *FoundType;
				}
			}
		}

		FNiagaraExt_StackItemReference InputRef = ModuleRef;
		InputRef.InputNameStack.Add(FName(*Pair.Key));
		// [VERIFIED: SetStackInputData — NiagaraExternalSystemEditorUtilities.h:1377]
		UNiagaraExternalEditUtilities::SetStackInputData(InputRef, NewValue, Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			OutWarnings.Add(ContextErrorsToString(Context));
			Context.Errors.Reset();
		}
		else
		{
			OutApplied.Add(FString::Printf(
				TEXT("%s/%s/%s.%s"),
				*EmitterName, *ScriptUsage, *ModuleName.ToString(), *Pair.Key));
		}
	}
}
