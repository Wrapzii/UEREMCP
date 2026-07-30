#include "UeremcpRefResolve.h"

bool FUeremcpRefResolve::IsRefObject(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid() || Value->Type != EJson::Object)
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Obj = Value->AsObject();
	if (!Obj.IsValid() || !Obj->HasField(TEXT("$ref")))
	{
		return false;
	}
	// Strict: only {"$ref": "..."} — extra keys make it not a substitution object.
	return Obj->Values.Num() == 1;
}

bool FUeremcpRefResolve::IsDollarStringRef(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid() || Value->Type != EJson::String)
	{
		return false;
	}
	const FString S = Value->AsString();
	if (S.Len() < 2 || S[0] != TEXT('$'))
	{
		return false;
	}
	for (int32 I = 1; I < S.Len(); ++I)
	{
		const TCHAR C = S[I];
		const bool bOk = (C >= TEXT('a') && C <= TEXT('z'))
			|| (C >= TEXT('A') && C <= TEXT('Z'))
			|| (C >= TEXT('0') && C <= TEXT('9'))
			|| C == TEXT('_') || C == TEXT('-');
		if (!bOk)
		{
			return false;
		}
	}
	return true;
}

bool FUeremcpRefResolve::ParseObjectRefPath(
	const FString& Ref,
	FString& OutOperationId,
	TArray<FString>& OutPath,
	FString& OutError)
{
	OutOperationId.Reset();
	OutPath.Reset();
	OutError.Reset();

	if (Ref.IsEmpty())
	{
		OutError = TEXT("$ref string is empty");
		return false;
	}

	TArray<FString> Parts;
	Ref.ParseIntoArray(Parts, TEXT("."), true);
	if (Parts.Num() < 2)
	{
		OutError = FString::Printf(
			TEXT("$ref '%s' must be '<operation_id>.<dotted.path>'"), *Ref);
		return false;
	}

	OutOperationId = Parts[0];
	for (int32 I = 1; I < Parts.Num(); ++I)
	{
		if (Parts[I].IsEmpty())
		{
			OutError = FString::Printf(TEXT("$ref '%s' has empty path segment"), *Ref);
			return false;
		}
		OutPath.Add(Parts[I]);
	}
	return true;
}

TSharedPtr<FJsonValue> FUeremcpRefResolve::LookupPath(
	const TSharedPtr<FJsonObject>& Root,
	const TArray<FString>& Path,
	FString& OutError)
{
	if (!Root.IsValid())
	{
		OutError = TEXT("lookup root is null");
		return nullptr;
	}

	TSharedPtr<FJsonValue> Current = MakeShared<FJsonValueObject>(Root);
	for (const FString& Segment : Path)
	{
		if (!Current.IsValid())
		{
			OutError = FString::Printf(TEXT("path segment '%s' unresolved (null parent)"), *Segment);
			return nullptr;
		}

		if (Current->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> Obj = Current->AsObject();
			if (!Obj->HasField(Segment))
			{
				OutError = FString::Printf(TEXT("missing field '%s'"), *Segment);
				return nullptr;
			}
			Current = Obj->TryGetField(Segment);
		}
		else if (Current->Type == EJson::Array)
		{
			if (!Segment.IsNumeric())
			{
				OutError = FString::Printf(TEXT("array index '%s' is not numeric"), *Segment);
				return nullptr;
			}
			const TArray<TSharedPtr<FJsonValue>>& Arr = Current->AsArray();
			const int32 Index = FCString::Atoi(*Segment);
			if (Index < 0 || Index >= Arr.Num())
			{
				OutError = FString::Printf(TEXT("array index %d out of range"), Index);
				return nullptr;
			}
			Current = Arr[Index];
		}
		else
		{
			OutError = FString::Printf(
				TEXT("cannot traverse into non-container at segment '%s'"), *Segment);
			return nullptr;
		}
	}
	return Current;
}

TSharedPtr<FJsonValue> FUeremcpRefResolve::ResolveDollarShorthand(
	const FString& OperationId,
	const TSharedPtr<FJsonObject>& Completed,
	FString& OutError)
{
	if (!Completed.IsValid())
	{
		OutError = FString::Printf(TEXT("$%s: completed result is null"), *OperationId);
		return nullptr;
	}

	// 1) UEREMCP envelope: result.primary_asset
	if (Completed->HasTypedField<EJson::Object>(TEXT("result")))
	{
		const TSharedPtr<FJsonObject> Result = Completed->GetObjectField(TEXT("result"));
		FString Primary;
		if (Result.IsValid() && Result->TryGetStringField(TEXT("primary_asset"), Primary) && !Primary.IsEmpty())
		{
			return MakeShared<FJsonValueString>(Primary);
		}
	}

	// 2–3) REAgentTools step_results: label, then path
	// [VERIFIED: batch_workflow_tools.py:37-48]
	FString Label;
	if (Completed->TryGetStringField(TEXT("label"), Label) && !Label.IsEmpty())
	{
		return MakeShared<FJsonValueString>(Label);
	}
	FString Path;
	if (Completed->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty())
	{
		return MakeShared<FJsonValueString>(Path);
	}

	OutError = FString::Printf(
		TEXT("$%s has no result.primary_asset, label, or path"), *OperationId);
	return nullptr;
}

static bool ResolveValue(
	TSharedPtr<FJsonValue>& Value,
	const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
	FString& OutError);

static bool ResolveObject(
	const TSharedPtr<FJsonObject>& Obj,
	const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
	FString& OutError)
{
	TArray<FString> Keys;
	Obj->Values.GetKeys(Keys);
	for (const FString& Key : Keys)
	{
		TSharedPtr<FJsonValue> Child = Obj->TryGetField(Key);
		if (!ResolveValue(Child, CompletedResults, OutError))
		{
			return false;
		}
		Obj->SetField(Key, Child);
	}
	return true;
}

static bool ResolveValue(
	TSharedPtr<FJsonValue>& Value,
	const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
	FString& OutError)
{
	if (!Value.IsValid())
	{
		return true;
	}

	if (FUeremcpRefResolve::IsRefObject(Value))
	{
		const FString Ref = Value->AsObject()->GetStringField(TEXT("$ref"));
		FString OpId;
		TArray<FString> Path;
		if (!FUeremcpRefResolve::ParseObjectRefPath(Ref, OpId, Path, OutError))
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* ResultObj = CompletedResults.Find(OpId);
		if (ResultObj == nullptr || !ResultObj->IsValid())
		{
			OutError = FString::Printf(
				TEXT("$ref '%s': operation '%s' has no completed result"), *Ref, *OpId);
			return false;
		}
		TSharedPtr<FJsonValue> Resolved = FUeremcpRefResolve::LookupPath(*ResultObj, Path, OutError);
		if (!Resolved.IsValid())
		{
			OutError = FString::Printf(TEXT("$ref '%s': %s"), *Ref, *OutError);
			return false;
		}
		Value = Resolved;
		return true;
	}

	if (FUeremcpRefResolve::IsDollarStringRef(Value))
	{
		const FString OpId = Value->AsString().Mid(1);
		const TSharedPtr<FJsonObject>* ResultObj = CompletedResults.Find(OpId);
		if (ResultObj == nullptr || !ResultObj->IsValid())
		{
			OutError = FString::Printf(
				TEXT("$%s: operation has no completed result"), *OpId);
			return false;
		}
		TSharedPtr<FJsonValue> Resolved =
			FUeremcpRefResolve::ResolveDollarShorthand(OpId, *ResultObj, OutError);
		if (!Resolved.IsValid())
		{
			return false;
		}
		Value = Resolved;
		return true;
	}

	if (Value->Type == EJson::Object)
	{
		return ResolveObject(Value->AsObject(), CompletedResults, OutError);
	}

	if (Value->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> Arr = Value->AsArray();
		for (TSharedPtr<FJsonValue>& Item : Arr)
		{
			if (!ResolveValue(Item, CompletedResults, OutError))
			{
				return false;
			}
		}
		Value = MakeShared<FJsonValueArray>(Arr);
		return true;
	}

	return true;
}

bool FUeremcpRefResolve::ResolveInPlace(
	TSharedPtr<FJsonValue>& Specification,
	const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
	FString& OutError)
{
	OutError.Reset();
	if (!Specification.IsValid())
	{
		return true;
	}

	TSharedPtr<FJsonValue> Working = Specification;
	if (!ResolveValue(Working, CompletedResults, OutError))
	{
		return false;
	}
	Specification = Working;
	return true;
}
