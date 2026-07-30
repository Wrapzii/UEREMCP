// UEREMCP — Inspect graph mapping helpers (WS-07).

#include "UeremcpNiagaraInspectMapping.h"

#include "NiagaraExternalSystemEditorUtilities.h"

namespace
{
	FString MakeEventHandlerKey(const FName& EmitterName, const FName& ScriptName, const FString& DisplayPath)
	{
		return FString::Printf(TEXT("%s|%s|%s"), *EmitterName.ToString(), *ScriptName.ToString(), *DisplayPath);
	}

	TSharedPtr<FJsonObject> MakeEventHandlerPlaceholder(
		const FName& EmitterName,
		const FName& ScriptName,
		const FString& EventHint,
		const FString& InferredFrom)
	{
		TSharedPtr<FJsonObject> Handler = MakeShared<FJsonObject>();
		if (!EventHint.IsEmpty())
		{
			Handler->SetStringField(TEXT("event_name"), EventHint);
		}
		if (!EmitterName.IsNone())
		{
			Handler->SetStringField(TEXT("source_emitter"), EmitterName.ToString());
		}
		if (!ScriptName.IsNone())
		{
			Handler->SetStringField(TEXT("script_usage"), ScriptName.ToString());
		}
		Handler->SetStringField(TEXT("inferred_from"), InferredFrom);
		Handler->SetStringField(
			TEXT("fidelity_note"),
			TEXT("Modules empty: GetEmitterTopology omits ParticleEventScript stacks; placeholder only."));
		Handler->SetArrayField(TEXT("modules"), TArray<TSharedPtr<FJsonValue>>());
		return Handler;
	}

	FString ExtractEventHintFromDisplayPath(const FString& StackDisplayPath)
	{
		int32 HandlerIdx = StackDisplayPath.Find(TEXT("Event Handler"), ESearchCase::IgnoreCase);
		if (HandlerIdx == INDEX_NONE)
		{
			return FString();
		}
		FString Tail = StackDisplayPath.Mid(HandlerIdx);
		Tail.ReplaceInline(TEXT("Event Handler"), TEXT(""));
		Tail.TrimStartAndEndInline();
		Tail.RemoveFromStart(TEXT("-"));
		Tail.RemoveFromStart(TEXT(":"));
		Tail.TrimStartAndEndInline();
		return Tail;
	}
}

TArray<TSharedPtr<FJsonValue>> FUeremcpNiagaraInspectMapping::BuildEventHandlerPlaceholders(
	const FNiagaraExt_StackIssues& Issues,
	const FNiagaraExt_SystemCompileState& CompileState)
{
	TArray<TSharedPtr<FJsonValue>> Handlers;
	TSet<FString> Seen;

	auto AddHandler = [&](const TSharedPtr<FJsonObject>& Handler)
	{
		if (!Handler.IsValid())
		{
			return;
		}
		FString Emitter;
		FString Script;
		FString Hint;
		Handler->TryGetStringField(TEXT("source_emitter"), Emitter);
		Handler->TryGetStringField(TEXT("script_usage"), Script);
		Handler->TryGetStringField(TEXT("event_name"), Hint);
		const FString Key = MakeEventHandlerKey(FName(*Emitter), FName(*Script), Hint);
		if (Seen.Contains(Key))
		{
			return;
		}
		Seen.Add(Key);
		Handlers.Add(MakeShared<FJsonValueObject>(Handler));
	};

	for (const FNiagaraExt_StackIssue& Issue : Issues.Issues)
	{
		const bool bEventPath = Issue.StackDisplayPath.Contains(TEXT("Event Handler"), ESearchCase::IgnoreCase)
			|| Issue.StackDisplayPath.Contains(TEXT("ParticleEventScript"), ESearchCase::IgnoreCase)
			|| Issue.ShortDescription.Contains(TEXT("Event Handler"), ESearchCase::IgnoreCase)
			|| Issue.Location.ScriptName == FName(TEXT("ParticleEventScript"));

		if (!bEventPath)
		{
			continue;
		}

		const FString Hint = ExtractEventHintFromDisplayPath(Issue.StackDisplayPath);
		AddHandler(MakeEventHandlerPlaceholder(
			Issue.Location.EmitterName,
			Issue.Location.ScriptName.IsNone() ? FName(TEXT("ParticleEventScript")) : Issue.Location.ScriptName,
			Hint,
			TEXT("GetStackIssues")));
	}

	for (const FNiagaraExt_ScriptCompileInfo& ScriptInfo : CompileState.Scripts)
	{
		if (ScriptInfo.ScriptName != FName(TEXT("ParticleEventScript")))
		{
			continue;
		}

		AddHandler(MakeEventHandlerPlaceholder(
			ScriptInfo.EmitterName,
			ScriptInfo.ScriptName,
			FString(),
			TEXT("GetSystemCompileState.per_script")));
	}

	return Handlers;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraInspectMapping::FindSystemGraph(
	const TArray<TSharedPtr<FJsonValue>>& Graphs)
{
	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		if (Graph->TryGetStringField(TEXT("graph_type"), GraphType)
			&& GraphType == TEXT("NiagaraSystemGraph"))
		{
			return Graph;
		}
	}
	return nullptr;
}

int32 FUeremcpNiagaraInspectMapping::CountEmitterGraphs(const TArray<TSharedPtr<FJsonValue>>& Graphs)
{
	int32 Count = 0;
	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		if (Graph->TryGetStringField(TEXT("graph_type"), GraphType)
			&& GraphType == TEXT("NiagaraEmitterGraph"))
		{
			++Count;
		}
	}
	return Count;
}

TArray<FString> FUeremcpNiagaraInspectMapping::ReadUserParameterNames(
	const TSharedPtr<FJsonObject>& SystemGraph)
{
	TArray<FString> Names;
	if (!SystemGraph.IsValid())
	{
		return Names;
	}

	const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
	if (SystemGraph->TryGetArrayField(TEXT("variables"), Variables) && Variables)
	{
		for (const TSharedPtr<FJsonValue>& VarValue : *Variables)
		{
			const TSharedPtr<FJsonObject> VarObj = VarValue->AsObject();
			if (!VarObj.IsValid())
			{
				continue;
			}
			FString Name;
			if (VarObj->TryGetStringField(TEXT("name"), Name))
			{
				Names.Add(Name);
			}
		}
	}

	const TSharedPtr<FJsonObject>* Extensions = nullptr;
	if (SystemGraph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions && Extensions->IsValid())
	{
		const TSharedPtr<FJsonObject>* Niagara = nullptr;
		if ((*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) && Niagara && Niagara->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* UserParams = nullptr;
			if ((*Niagara)->TryGetArrayField(TEXT("user_parameters"), UserParams) && UserParams)
			{
				for (const TSharedPtr<FJsonValue>& ParamValue : *UserParams)
				{
					const TSharedPtr<FJsonObject> ParamObj = ParamValue->AsObject();
					if (!ParamObj.IsValid())
					{
						continue;
					}
					FString Name;
					if (ParamObj->TryGetStringField(TEXT("name"), Name) && !Names.Contains(Name))
					{
						Names.Add(Name);
					}
				}
			}
		}
	}

	return Names;
}
