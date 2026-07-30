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

bool FUeremcpRefResolve::ParseRefString(
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

static bool ResolveValue(
	TSharedPtr<FJsonValue>& Value,
	const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
	FString& OutError);

static bool ResolveObject(
	const TSharedPtr<FJsonObject>& Obj,
	const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
	FString& OutError)
{
	// Collect keys first — we may replace values.
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
		if (!FUeremcpRefResolve::ParseRefString(Ref, OpId, Path, OutError))
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* ResultObj = CompletedResults.Find(OpId);
		if (ResultObj == nullptr || !ResultObj->IsValid())
		{
			OutError = FString::Printf(TEXT("$ref '%s': operation '%s' has no completed result"), *Ref, *OpId);
			return false;
		}
		TSharedPtr<FJsonValue> Resolved = FUeremcpRefResolve::LookupPath(*ResultObj, Path, OutError);
		if (!Resolved.IsValid())
		{
			OutError = FString::Printf(TEXT("$ref '%s': %s"), *Ref, *OutError);
			return false;
		}
		// Deep-copy via re-serialise would be safer against shared mutation; for v1
		// share the value pointer — callers must treat completed results as immutable.
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

	// Work on a clone of the top-level structure so failure leaves the caller's
	// original pointer untouched. Nested objects are still mutated via shared
	// pointers after we decide to commit — clone the root object/array shallowly
	// by re-walking into a fresh tree is expensive; instead copy the incoming
	// pointer, resolve, and only assign back on success via the same reference.
	TSharedPtr<FJsonValue> Working = Specification;
	if (!ResolveValue(Working, CompletedResults, OutError))
	{
		return false;
	}
	Specification = Working;
	return true;
}
