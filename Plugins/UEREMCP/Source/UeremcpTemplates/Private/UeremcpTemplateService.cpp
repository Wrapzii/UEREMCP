// UEREMCP — template service implementation. Owner: WS-15.

#include "UeremcpTemplateService.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString Lower(const FString& Value)
	{
		return Value.ToLower();
	}

	bool ContainsInsensitive(const FString& Haystack, const FString& Needle)
	{
		return Lower(Haystack).Contains(Lower(Needle));
	}

	TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& Source)
	{
		if (!Source.IsValid())
		{
			return nullptr;
		}

		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		if (!FJsonSerializer::Serialize(Source.ToSharedRef(), Writer))
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Clone;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
		FJsonSerializer::Deserialize(Reader, Clone);
		return Clone;
	}

	TSharedPtr<FJsonValue> CloneJsonValue(const TSharedPtr<FJsonValue>& Source)
	{
		if (!Source.IsValid())
		{
			return nullptr;
		}

		if (Source->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ClonedObject = CloneJsonObject(Source->AsObject());
			if (!ClonedObject.IsValid())
			{
				return nullptr;
			}
			return MakeShared<FJsonValueObject>(ClonedObject);
		}

		if (Source->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> ClonedArray;
			for (const TSharedPtr<FJsonValue>& Entry : Source->AsArray())
			{
				ClonedArray.Add(CloneJsonValue(Entry));
			}
			return MakeShared<FJsonValueArray>(MoveTemp(ClonedArray));
		}

		switch (Source->Type)
		{
		case EJson::String:
			return MakeShared<FJsonValueString>(Source->AsString());
		case EJson::Number:
			return MakeShared<FJsonValueNumber>(Source->AsNumber());
		case EJson::Boolean:
			return MakeShared<FJsonValueBoolean>(Source->AsBool());
		default:
			return MakeShared<FJsonValueNull>();
		}
	}
}

FUeremcpTemplateService::FUeremcpTemplateService(FUeremcpTemplateStore& InStore)
	: Store(InStore)
{
}

TArray<FUeremcpTemplateSearchHit> FUeremcpTemplateService::Search(const FUeremcpTemplateSearchQuery& Query) const
{
	TArray<FString> Ids;
	Store.GetAllIds(Ids);

	TArray<FUeremcpTemplateSearchHit> Hits;
	for (const FString& TemplateId : Ids)
	{
		const FUeremcpTemplateRecord* Record = Store.FindById(TemplateId);
		if (!Record)
		{
			continue;
		}

		if (!Query.Domain.IsEmpty() && !Record->Domain.Equals(Query.Domain, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!PassesElementFilter(*Record, Query.Element))
		{
			continue;
		}

		const float Score = ScoreRecord(*Record, Query);
		if (!Query.Query.IsEmpty() && Score <= 0.f)
		{
			continue;
		}

		FUeremcpTemplateSearchHit Hit;
		Hit.TemplateId = Record->TemplateId;
		Hit.Domain = Record->Domain;
		Hit.Category = Record->Category;
		Hit.Description = Record->Description;
		Hit.Score = Score;
		Hits.Add(MoveTemp(Hit));
	}

	Hits.Sort([](const FUeremcpTemplateSearchHit& A, const FUeremcpTemplateSearchHit& B)
	{
		if (!FMath::IsNearlyEqual(A.Score, B.Score))
		{
			return A.Score > B.Score;
		}
		return A.TemplateId < B.TemplateId;
	});

	const int32 Limit = FMath::Clamp(Query.Limit, 1, 100);
	if (Hits.Num() > Limit)
	{
		Hits.SetNum(Limit);
	}

	return Hits;
}

FUeremcpTemplateInstantiateResult FUeremcpTemplateService::Instantiate(
	const FUeremcpTemplateInstantiateRequest& Request) const
{
	FUeremcpTemplateInstantiateResult Result;
	Result.Status = TEXT("failed_validation");

	const FUeremcpTemplateRecord* Record = Store.FindById(Request.TemplateId);
	if (!Record)
	{
		Result.Summary = FString::Printf(TEXT("Unknown template_id '%s'."), *Request.TemplateId);
		return Result;
	}

	if (Request.Modifiers.IsValid())
	{
		static const TCHAR* BucketNames[] = { TEXT("replace"), TEXT("adjust"), TEXT("add"), TEXT("preserve") };
		for (int32 BucketIndex = 0; BucketIndex < UE_ARRAY_COUNT(BucketNames); ++BucketIndex)
		{
			const TArray<TSharedPtr<FJsonValue>>* ModifierValues = nullptr;
			if (!Request.Modifiers->TryGetArrayField(BucketNames[BucketIndex], ModifierValues) || !ModifierValues)
			{
				continue;
			}

			for (const TSharedPtr<FJsonValue>& Value : *ModifierValues)
			{
				FString ModifierName;
				if (!Value.IsValid() || !Value->TryGetString(ModifierName))
				{
					continue;
				}

				if (!Record->SupportedModifiers.Contains(ModifierName))
				{
					Result.Summary = FString::Printf(
						TEXT("Unsupported modifier '%s' for template '%s'."),
						*ModifierName,
						*Request.TemplateId);
					return Result;
				}
			}
		}
	}

	FString MaterializeError;
	const TSharedPtr<FJsonObject> Plan = MaterializePlan(*Record, Request.Inputs, Request.Modifiers, MaterializeError);
	if (!Plan.IsValid())
	{
		Result.Summary = MaterializeError;
		return Result;
	}

	Result.bSuccess = true;
	Result.Status = TEXT("partially_completed");
	Result.Summary = FString::Printf(
		TEXT("Materialized construction_plan for '%s'. execute_plan delegation not wired in v1."),
		*Request.TemplateId);
	Result.CapabilityNotes.Add(TEXT("instantiate_template v1 returns materialized plan only; execute_plan not invoked."));
	Result.MaterializedPlan = Plan;
	return Result;
}

float FUeremcpTemplateService::ScoreRecord(
	const FUeremcpTemplateRecord& Record,
	const FUeremcpTemplateSearchQuery& Query)
{
	if (Query.Query.IsEmpty())
	{
		return 1.f;
	}

	const FString Needle = Query.Query;
	float Score = 0.f;

	if (ContainsInsensitive(Record.TemplateId, Needle))
	{
		Score += 3.f;
	}
	if (ContainsInsensitive(Record.Description, Needle))
	{
		Score += 2.f;
	}
	if (ContainsInsensitive(Record.Category, Needle))
	{
		Score += 1.f;
	}
	for (const FString& Term : Record.SearchTerms)
	{
		if (ContainsInsensitive(Term, Needle))
		{
			Score += 1.5f;
		}
	}

	return Score;
}

bool FUeremcpTemplateService::PassesElementFilter(
	const FUeremcpTemplateRecord& Record,
	const FString& ElementFilter)
{
	if (ElementFilter.IsEmpty() || ElementFilter.Equals(TEXT("any"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (Record.DeclaredElement.IsEmpty())
	{
		return true;
	}

	if (!Record.Document.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Inputs = nullptr;
	if (!Record.Document->TryGetObjectField(TEXT("inputs"), Inputs) || !Inputs || !Inputs->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!(*Inputs)->TryGetObjectField(TEXT("properties"), Properties) || !Properties || !Properties->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Element = nullptr;
	if (!(*Properties)->TryGetObjectField(TEXT("element"), Element) || !Element || !Element->IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EnumValues = nullptr;
	if (!(*Element)->TryGetArrayField(TEXT("enum"), EnumValues) || !EnumValues)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Value : *EnumValues)
	{
		FString EnumEntry;
		if (Value.IsValid() && Value->TryGetString(EnumEntry)
			&& EnumEntry.Equals(ElementFilter, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

TSharedPtr<FJsonValue> FUeremcpTemplateService::ApplyInputsToJsonValue(
	const TSharedPtr<FJsonValue>& Value,
	const TSharedPtr<FJsonObject>& Inputs)
{
	if (!Value.IsValid())
	{
		return nullptr;
	}

	if (Value->Type == EJson::String)
	{
		const FString StringValue = Value->AsString();
		if (StringValue.StartsWith(TEXT("{{inputs.")) && StringValue.EndsWith(TEXT("}}")))
		{
			const FString Key = StringValue.Mid(9, StringValue.Len() - 11);
			if (Inputs.IsValid())
			{
				const TSharedPtr<FJsonValue> InputValue = Inputs->TryGetField(Key);
				if (InputValue.IsValid())
				{
					return CloneJsonValue(InputValue);
				}
			}
			return MakeShared<FJsonValueNull>();
		}
		return MakeShared<FJsonValueString>(StringValue);
	}

	if (Value->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> SourceObject = Value->AsObject();
		const TSharedPtr<FJsonObject> OutObject = MakeShared<FJsonObject>();
		for (const auto& Field : SourceObject->Values)
		{
			OutObject->SetField(FString(Field.Key), ApplyInputsToJsonValue(Field.Value, Inputs));
		}
		return MakeShared<FJsonValueObject>(OutObject);
	}

	if (Value->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> OutArray;
		for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
		{
			OutArray.Add(ApplyInputsToJsonValue(Entry, Inputs));
		}
		return MakeShared<FJsonValueArray>(MoveTemp(OutArray));
	}

	return CloneJsonValue(Value);
}

TSharedPtr<FJsonObject> FUeremcpTemplateService::MaterializePlan(
	const FUeremcpTemplateRecord& Record,
	const TSharedPtr<FJsonObject>& Inputs,
	const TSharedPtr<FJsonObject>& Modifiers,
	FString& OutError)
{
	if (!Record.Document.IsValid())
	{
		OutError = TEXT("Template document missing.");
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* PlanSteps = nullptr;
	if (!Record.Document->TryGetArrayField(TEXT("construction_plan"), PlanSteps) || !PlanSteps)
	{
		OutError = TEXT("Template has no construction_plan.");
		return nullptr;
	}

	const TSharedPtr<FJsonValue> MaterializedStepsValue = ApplyInputsToJsonValue(
		MakeShared<FJsonValueArray>(*PlanSteps),
		Inputs);

	const TArray<TSharedPtr<FJsonValue>>* MaterializedSteps = nullptr;
	if (!MaterializedStepsValue.IsValid()
		|| MaterializedStepsValue->Type != EJson::Array
		|| !(MaterializedSteps = &MaterializedStepsValue->AsArray()))
	{
		OutError = TEXT("Failed to materialize construction_plan.");
		return nullptr;
	}

	const TSharedPtr<FJsonObject> Plan = MakeShared<FJsonObject>();
	Plan->SetStringField(TEXT("template_id"), Record.TemplateId);
	Plan->SetArrayField(TEXT("operations"), *MaterializedSteps);

	if (Inputs.IsValid())
	{
		Plan->SetObjectField(TEXT("resolved_inputs"), Inputs);
	}
	if (Modifiers.IsValid())
	{
		Plan->SetObjectField(TEXT("applied_modifiers"), Modifiers);
	}

	Plan->SetStringField(TEXT("executor"), TEXT("execute_plan"));
	Plan->SetStringField(TEXT("status_note"), TEXT("materialized_only_v1"));
	return Plan;
}
