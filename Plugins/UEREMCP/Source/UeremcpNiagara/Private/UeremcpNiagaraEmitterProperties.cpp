// UEREMCP — Emitter Properties + Emitter State Life Cycle (WS-07).

#include "UeremcpNiagaraEmitterProperties.h"

#include "UeremcpNiagaraStackInputs.h"

#include "NiagaraScript.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

	FName FindEmitterStateModuleName(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		int32& InOutOps)
	{
		FNiagaraExt_StackItemReference EmitterRef(System, FName(*EmitterName));
		FNiagaraExt_EmitterTopology Topology;
		UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, Topology, Context);
		++InOutOps;
		if (Context.HasErrors())
		{
			Context.Errors.Reset();
			return NAME_None;
		}

		for (const FNiagaraExt_ModuleTopology& Mod : Topology.EmitterUpdateScript.Modules)
		{
			const FString Name = Mod.ModuleName.ToString();
			if (Name.Equals(TEXT("EmitterState"), ESearchCase::IgnoreCase)
				|| Name.Equals(TEXT("Emitter State"), ESearchCase::IgnoreCase)
				|| (Mod.ModuleScript
					&& Mod.ModuleScript->GetName().Contains(TEXT("EmitterState"))))
			{
				return Mod.ModuleName;
			}
		}
		return NAME_None;
	}

	FString StackInputValueToString(const FNiagaraExt_StackInputValue& Value)
	{
		if (const FNiagaraFloat* AsFloat = Value.GetPtr<FNiagaraFloat>())
		{
			return FString::SanitizeFloat(AsFloat->Value);
		}
		if (const FNiagaraExt_StackInputData_Enum* EnumVal =
			Value.GetPtr<FNiagaraExt_StackInputData_Enum>())
		{
			return EnumVal->EnumName.ToString();
		}
		if (const FNiagaraBool* AsBool = Value.GetPtr<FNiagaraBool>())
		{
			return AsBool->GetValue() ? TEXT("true") : TEXT("false");
		}
		if (const FNiagaraExt_StackInputData_Linked* Linked =
			Value.GetPtr<FNiagaraExt_StackInputData_Linked>())
		{
			return Linked->LinkedVariable.Name.ToString();
		}
		return FString();
	}

	void SetEmitterStateInput(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		const FString& EmitterName,
		const FName& ModuleName,
		const FString& InputName,
		const TSharedPtr<FJsonValue>& JsonVal,
		int32& InOutOps,
		TArray<FString>& OutApplied,
		TArray<FString>& OutWarnings)
	{
		TSharedPtr<FJsonObject> Single = MakeShared<FJsonObject>();
		Single->SetField(InputName, JsonVal);
		TArray<FString> Applied;
		FUeremcpNiagaraStackInputs::ApplyModuleInputs(
			System,
			Context,
			EmitterName,
			TEXT("EmitterUpdateScript"),
			ModuleName,
			Single,
			InOutOps,
			OutWarnings,
			Applied);
		OutApplied.Append(Applied);
	}
}

void FUeremcpNiagaraEmitterProperties::ParseFromJsonObject(
	const TSharedPtr<FJsonObject>& Obj,
	FUeremcpNiagaraEmitterPropertyPlan& InOutPlan)
{
	if (!Obj.IsValid())
	{
		return;
	}

	FString SimTargetRaw;
	if (Obj->TryGetStringField(TEXT("sim_target"), SimTargetRaw)
		|| Obj->TryGetStringField(TEXT("SimTarget"), SimTargetRaw))
	{
		InOutPlan.SimTarget = NormalizeSimTarget(SimTargetRaw);
	}

	if (Obj->HasField(TEXT("enabled")))
	{
		InOutPlan.bEnabled = Obj->GetBoolField(TEXT("enabled"));
		InOutPlan.bHasEnabled = true;
	}
	else if (Obj->HasField(TEXT("bEnabled")))
	{
		InOutPlan.bEnabled = Obj->GetBoolField(TEXT("bEnabled"));
		InOutPlan.bHasEnabled = true;
	}
	else if (Obj->HasField(TEXT("bIsEnabled")))
	{
		InOutPlan.bEnabled = Obj->GetBoolField(TEXT("bIsEnabled"));
		InOutPlan.bHasEnabled = true;
	}

	const TSharedPtr<FJsonObject>* LifeCycle = nullptr;
	if (Obj->TryGetObjectField(TEXT("life_cycle"), LifeCycle) && LifeCycle && (*LifeCycle).IsValid())
	{
		(*LifeCycle)->TryGetStringField(TEXT("mode"), InOutPlan.LifeCycleMode);
		if (InOutPlan.LifeCycleMode.IsEmpty())
		{
			(*LifeCycle)->TryGetStringField(TEXT("life_cycle_mode"), InOutPlan.LifeCycleMode);
		}
		(*LifeCycle)->TryGetStringField(TEXT("loop_behavior"), InOutPlan.LoopBehavior);
		(*LifeCycle)->TryGetStringField(TEXT("inactive_response"), InOutPlan.InactiveResponse);
		if ((*LifeCycle)->HasField(TEXT("loop_duration")))
		{
			InOutPlan.LoopDuration = static_cast<float>((*LifeCycle)->GetNumberField(TEXT("loop_duration")));
		}
	}

	// Flat aliases matching Details panel labels.
	FString Flat;
	if (Obj->TryGetStringField(TEXT("life_cycle_mode"), Flat) && InOutPlan.LifeCycleMode.IsEmpty())
	{
		InOutPlan.LifeCycleMode = Flat;
	}
	if (Obj->TryGetStringField(TEXT("loop_behavior"), Flat) && InOutPlan.LoopBehavior.IsEmpty())
	{
		InOutPlan.LoopBehavior = Flat;
	}
	if (Obj->TryGetStringField(TEXT("inactive_response"), Flat) && InOutPlan.InactiveResponse.IsEmpty())
	{
		InOutPlan.InactiveResponse = Flat;
	}
	if (Obj->HasField(TEXT("loop_duration")) && !InOutPlan.LoopDuration.IsSet())
	{
		InOutPlan.LoopDuration = static_cast<float>(Obj->GetNumberField(TEXT("loop_duration")));
	}
}

FString FUeremcpNiagaraEmitterProperties::NormalizeSimTarget(const FString& Raw)
{
	const FString Trimmed = Raw.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FString();
	}
	if (Trimmed.Equals(TEXT("CPUSim"), ESearchCase::IgnoreCase)
		|| Trimmed.Equals(TEXT("CPU"), ESearchCase::IgnoreCase)
		|| Trimmed.Equals(TEXT("0"), ESearchCase::IgnoreCase))
	{
		return TEXT("CPUSim");
	}
	if (Trimmed.Equals(TEXT("GPUComputeSim"), ESearchCase::IgnoreCase)
		|| Trimmed.Equals(TEXT("GPU"), ESearchCase::IgnoreCase)
		|| Trimmed.Equals(TEXT("1"), ESearchCase::IgnoreCase))
	{
		return TEXT("GPUComputeSim");
	}
	return FString();
}

bool FUeremcpNiagaraEmitterProperties::ApplySimTargetAndEnabled(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	const FString& EmitterName,
	const FUeremcpNiagaraEmitterPropertyPlan& Plan,
	int32& InOutOps,
	TArray<FString>& OutApplied,
	TArray<FString>& OutWarnings)
{
	if (!System || (Plan.SimTarget.IsEmpty() && !Plan.bHasEnabled))
	{
		return true;
	}

	FNiagaraExt_StackItemReference EmitterRef(System, FName(*EmitterName));
	FNiagaraExt_EmitterData Data;
	UNiagaraExternalEditUtilities::GetEmitterData(EmitterRef, Data, Context);
	++InOutOps;
	if (Context.HasErrors())
	{
		OutWarnings.Add(ContextErrorsToString(Context));
		Context.Errors.Reset();
		return false;
	}

	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	if (!Data.PropertyValues.IsEmpty())
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data.PropertyValues);
		FJsonSerializer::Deserialize(Reader, Props);
	}
	if (!Props.IsValid())
	{
		Props = MakeShared<FJsonObject>();
	}

	if (!Plan.SimTarget.IsEmpty())
	{
		// FJsonObjectConverter expects enum as string matching UENUM names.
		// [VERIFIED: SetEmitterHandleAndDataProperties → JsonObjectToUStruct FVersionedNiagaraEmitterData]
		Props->SetStringField(TEXT("SimTarget"), Plan.SimTarget);
	}
	if (Plan.bHasEnabled)
	{
		Props->SetBoolField(TEXT("bIsEnabled"), Plan.bEnabled);
	}

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Props.ToSharedRef(), Writer);
	Data.PropertyValues = Serialized;

	// [VERIFIED: SetEmitterData — NiagaraExternalSystemEditorUtilities.h:1375]
	UNiagaraExternalEditUtilities::SetEmitterData(EmitterRef, Data, Context);
	++InOutOps;
	if (Context.HasErrors())
	{
		OutWarnings.Add(ContextErrorsToString(Context));
		Context.Errors.Reset();
		return false;
	}

	if (!Plan.SimTarget.IsEmpty())
	{
		OutApplied.Add(FString::Printf(TEXT("%s.SimTarget=%s"), *EmitterName, *Plan.SimTarget));
	}
	if (Plan.bHasEnabled)
	{
		OutApplied.Add(FString::Printf(
			TEXT("%s.bIsEnabled=%s"),
			*EmitterName,
			Plan.bEnabled ? TEXT("true") : TEXT("false")));
	}
	return true;
}

bool FUeremcpNiagaraEmitterProperties::ApplyLifeCycleViaEmitterState(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	const FString& EmitterName,
	const FUeremcpNiagaraEmitterPropertyPlan& Plan,
	int32& InOutOps,
	TArray<FString>& OutApplied,
	TArray<FString>& OutWarnings)
{
	if (!System || !Plan.HasLifeCycleFields())
	{
		return true;
	}

	const FName ModuleName = FindEmitterStateModuleName(System, Context, EmitterName, InOutOps);
	if (ModuleName.IsNone())
	{
		OutWarnings.Add(FString::Printf(
			TEXT("%s: Life Cycle write skipped — no Emitter State module on EmitterUpdateScript "
				 "(add primitive_id=emitter_state first)"),
			*EmitterName));
		return false;
	}

	if (!Plan.LifeCycleMode.IsEmpty())
	{
		TSharedPtr<FJsonObject> ModeObj = MakeShared<FJsonObject>();
		ModeObj->SetStringField(TEXT("mode"), TEXT("enum"));
		ModeObj->SetStringField(TEXT("enum_value"), Plan.LifeCycleMode);
		SetEmitterStateInput(
			System, Context, EmitterName, ModuleName, TEXT("Life Cycle Mode"),
			MakeShared<FJsonValueObject>(ModeObj),
			InOutOps, OutApplied, OutWarnings);
	}
	if (!Plan.LoopBehavior.IsEmpty())
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("mode"), TEXT("enum"));
		O->SetStringField(TEXT("enum_value"), Plan.LoopBehavior);
		SetEmitterStateInput(
			System, Context, EmitterName, ModuleName, TEXT("Loop Behavior"),
			MakeShared<FJsonValueObject>(O),
			InOutOps, OutApplied, OutWarnings);
	}
	if (Plan.LoopDuration.IsSet())
	{
		SetEmitterStateInput(
			System, Context, EmitterName, ModuleName, TEXT("Loop Duration"),
			MakeShared<FJsonValueNumber>(Plan.LoopDuration.GetValue()),
			InOutOps, OutApplied, OutWarnings);
	}
	if (!Plan.InactiveResponse.IsEmpty())
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("mode"), TEXT("enum"));
		O->SetStringField(TEXT("enum_value"), Plan.InactiveResponse);
		SetEmitterStateInput(
			System, Context, EmitterName, ModuleName, TEXT("Inactive Response"),
			MakeShared<FJsonValueObject>(O),
			InOutOps, OutApplied, OutWarnings);
	}
	return true;
}

void FUeremcpNiagaraEmitterProperties::ApplyAll(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	const FString& EmitterName,
	const FUeremcpNiagaraEmitterPropertyPlan& Plan,
	int32& InOutOps,
	TArray<FString>& OutApplied,
	TArray<FString>& OutWarnings)
{
	if (!Plan.HasAny())
	{
		return;
	}
	ApplySimTargetAndEnabled(System, Context, EmitterName, Plan, InOutOps, OutApplied, OutWarnings);
	ApplyLifeCycleViaEmitterState(System, Context, EmitterName, Plan, InOutOps, OutApplied, OutWarnings);
}

TSharedPtr<FJsonObject> FUeremcpNiagaraEmitterProperties::ReadLifeCycleFromEmitterState(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	const FString& EmitterName,
	int32& InOutOps)
{
	if (!System)
	{
		return nullptr;
	}

	const FName ModuleName = FindEmitterStateModuleName(System, Context, EmitterName, InOutOps);
	if (ModuleName.IsNone())
	{
		return nullptr;
	}

	FNiagaraExt_StackItemReference ModuleRef(
		System, FName(*EmitterName), FName(TEXT("EmitterUpdateScript")), ModuleName);
	FNiagaraExt_ModuleInputValues Values;
	UNiagaraExternalEditUtilities::GetModuleInputValues(ModuleRef, Values, Context);
	++InOutOps;
	if (Context.HasErrors())
	{
		Context.Errors.Reset();
		return nullptr;
	}

	TSharedPtr<FJsonObject> LifeCycle = MakeShared<FJsonObject>();
	bool bAny = false;
	for (const FNiagaraExt_StackInputValueEntry& Entry : Values.Inputs)
	{
		const FString Name = Entry.Name.ToString();
		const FString Str = StackInputValueToString(Entry.Value);
		if (Str.IsEmpty())
		{
			continue;
		}
		if (Name.Equals(TEXT("Life Cycle Mode"), ESearchCase::IgnoreCase))
		{
			LifeCycle->SetStringField(TEXT("mode"), Str);
			bAny = true;
		}
		else if (Name.Equals(TEXT("Loop Behavior"), ESearchCase::IgnoreCase))
		{
			LifeCycle->SetStringField(TEXT("loop_behavior"), Str);
			bAny = true;
		}
		else if (Name.Equals(TEXT("Loop Duration"), ESearchCase::IgnoreCase))
		{
			if (const FNiagaraFloat* AsFloat = Entry.Value.GetPtr<FNiagaraFloat>())
			{
				LifeCycle->SetNumberField(TEXT("loop_duration"), AsFloat->Value);
				bAny = true;
			}
			else
			{
				LifeCycle->SetStringField(TEXT("loop_duration"), Str);
				bAny = true;
			}
		}
		else if (Name.Equals(TEXT("Inactive Response"), ESearchCase::IgnoreCase))
		{
			LifeCycle->SetStringField(TEXT("inactive_response"), Str);
			bAny = true;
		}
	}
	return bAny ? LifeCycle : nullptr;
}
