// UEREMCP — template service implementation. Owner: WS-15.

#include "UeremcpTemplateService.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
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

	void ApplyMergePatch(
		const TSharedPtr<FJsonObject>& Target,
		const TSharedPtr<FJsonObject>& Patch)
	{
		for (const auto& Field : Patch->Values)
		{
			if (!Field.Value.IsValid() || Field.Value->Type == EJson::Null)
			{
				Target->RemoveField(Field.Key);
				continue;
			}
			if (Field.Value->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> Child;
				const TSharedPtr<FJsonObject>* Existing = nullptr;
				if (Target->TryGetObjectField(Field.Key, Existing) && Existing && Existing->IsValid())
				{
					Child = CloneJsonObject(*Existing);
				}
				else
				{
					Child = MakeShared<FJsonObject>();
				}
				ApplyMergePatch(Child, Field.Value->AsObject());
				Target->SetObjectField(Field.Key, Child);
				continue;
			}
			Target->SetField(Field.Key, CloneJsonValue(Field.Value));
		}
	}

	int32 FindOperation(const TArray<TSharedPtr<FJsonValue>>& Operations, const FString& Id)
	{
		for (int32 Index = 0; Index < Operations.Num(); ++Index)
		{
			FString Candidate;
			if (Operations[Index].IsValid()
				&& Operations[Index]->Type == EJson::Object
				&& Operations[Index]->AsObject()->TryGetStringField(TEXT("id"), Candidate)
				&& Candidate == Id)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	bool ValidateOperationGraph(
		const TArray<TSharedPtr<FJsonValue>>& Operations,
		FString& OutError)
	{
		TSet<FString> Ids;
		for (const TSharedPtr<FJsonValue>& Value : Operations)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				OutError = TEXT("Materialized plan contains a non-object operation.");
				return false;
			}
			FString Id;
			FString Action;
			if (!Value->AsObject()->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty()
				|| !Value->AsObject()->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
			{
				OutError = TEXT("Materialized plan operation requires non-empty id and action.");
				return false;
			}
			if (Ids.Contains(Id))
			{
				OutError = FString::Printf(TEXT("Duplicate materialized operation id '%s'."), *Id);
				return false;
			}
			Ids.Add(Id);
		}
		for (const TSharedPtr<FJsonValue>& Value : Operations)
		{
			FString Id;
			Value->AsObject()->TryGetStringField(TEXT("id"), Id);
			const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
			if (!Value->AsObject()->TryGetArrayField(TEXT("depends_on"), Dependencies) || !Dependencies)
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Dependency : *Dependencies)
			{
				FString DependencyId;
				if (!Dependency.IsValid() || !Dependency->TryGetString(DependencyId)
					|| !Ids.Contains(DependencyId))
				{
					OutError = FString::Printf(
						TEXT("Operation '%s' depends on an unknown operation."),
						*Id);
					return false;
				}
			}
		}
		return true;
	}

	bool JsonScalarEquals(
		const TSharedPtr<FJsonValue>& Left,
		const TSharedPtr<FJsonValue>& Right)
	{
		if (!Left.IsValid() || !Right.IsValid() || Left->Type != Right->Type)
		{
			return false;
		}

		switch (Left->Type)
		{
		case EJson::String:
			return Left->AsString() == Right->AsString();
		case EJson::Number:
			return FMath::IsNearlyEqual(Left->AsNumber(), Right->AsNumber());
		case EJson::Boolean:
			return Left->AsBool() == Right->AsBool();
		case EJson::Null:
			return true;
		default:
			return false;
		}
	}

	bool IsTemplateIdToken(const FString& Token, bool bRequireLeadingLetter)
	{
		if (Token.IsEmpty() || (bRequireLeadingLetter && !FChar::IsAlpha(Token[0])))
		{
			return false;
		}
		for (const TCHAR Character : Token)
		{
			if (!(FChar::IsLower(Character) || FChar::IsDigit(Character) || Character == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsValidTemplateId(const FString& TemplateId)
	{
		TArray<FString> Tokens;
		TemplateId.ParseIntoArray(Tokens, TEXT("."), true);
		if (Tokens.Num() < 3 || !IsTemplateIdToken(Tokens[0], true))
		{
			return false;
		}
		for (int32 Index = 1; Index < Tokens.Num() - 1; ++Index)
		{
			if (!IsTemplateIdToken(Tokens[Index], false))
			{
				return false;
			}
		}
		const FString& Version = Tokens.Last();
		if (Version.Len() < 2 || Version[0] != TEXT('v'))
		{
			return false;
		}
		for (int32 Index = 1; Index < Version.Len(); ++Index)
		{
			if (!FChar::IsDigit(Version[Index]))
			{
				return false;
			}
		}
		return true;
	}

	FString SlugFromAssetPath(const FString& SourceAsset)
	{
		const FString SourceName = FPaths::GetBaseFilename(SourceAsset).ToLower();
		FString Slug;
		bool bPreviousUnderscore = false;
		for (const TCHAR Character : SourceName)
		{
			const bool bAllowed = FChar::IsLower(Character) || FChar::IsDigit(Character);
			if (bAllowed)
			{
				Slug.AppendChar(Character);
				bPreviousUnderscore = false;
			}
			else if (!bPreviousUnderscore && !Slug.IsEmpty())
			{
				Slug.AppendChar(TEXT('_'));
				bPreviousUnderscore = true;
			}
		}
		while (Slug.EndsWith(TEXT("_")))
		{
			Slug.LeftChopInline(1);
		}
		if (Slug.IsEmpty() || !FChar::IsAlpha(Slug[0]))
		{
			Slug = TEXT("asset_") + Slug;
		}
		return Slug;
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
	Result.InheritedFacts.Add(FString::Printf(TEXT("template_id=%s"), *Record->TemplateId));
	if (Record->Composes.Num() > 0)
	{
		Result.InheritedFacts.Add(FString::Printf(
			TEXT("composes=%s"),
			*FString::Join(Record->Composes, TEXT(","))));
	}

	TSharedPtr<FJsonObject> EffectiveInputs = CloneJsonObject(Request.Inputs);
	if (!EffectiveInputs.IsValid())
	{
		EffectiveInputs = MakeShared<FJsonObject>();
	}
	if (!Request.TargetAssetPath.IsEmpty() && !EffectiveInputs->HasField(TEXT("target_path")))
	{
		EffectiveInputs->SetStringField(TEXT("target_path"), Request.TargetAssetPath);
	}
	TArray<FString> InputNames;
	for (const auto& InputPair : EffectiveInputs->Values)
	{
		InputNames.Add(FString(InputPair.Key));
	}
	InputNames.Sort();
	for (const FString& InputName : InputNames)
	{
		if (InputName == TEXT("preset_material_parameters")
			|| InputName == TEXT("preset_niagara_parameters"))
		{
			continue;
		}
		Result.OverriddenFacts.Add(FString::Printf(TEXT("input=%s"), *InputName));
	}
	static const TCHAR* ModifierBuckets[] = { TEXT("replace"), TEXT("adjust"), TEXT("add"), TEXT("preserve") };
	for (const TCHAR* Bucket : ModifierBuckets)
	{
		const TArray<TSharedPtr<FJsonValue>>* RequestedModifiers = nullptr;
		if (!Request.Modifiers.IsValid()
			|| !Request.Modifiers->TryGetArrayField(Bucket, RequestedModifiers)
			|| !RequestedModifiers)
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& ModifierValue : *RequestedModifiers)
		{
			FString ModifierName;
			if (ModifierValue.IsValid() && ModifierValue->TryGetString(ModifierName))
			{
				Result.OverriddenFacts.Add(FString::Printf(
					TEXT("modifier.%s=%s"),
					Bucket,
					*ModifierName));
			}
		}
	}

	FString InputError;
	if (!ValidateInputs(*Record, EffectiveInputs, InputError))
	{
		Result.Summary = InputError;
		return Result;
	}

	FString Element;
	if (EffectiveInputs->TryGetStringField(TEXT("element"), Element))
	{
		const FUeremcpElementPreset* Preset = Store.FindElementPreset(Element);
		if (!Preset)
		{
			Result.Summary = FString::Printf(TEXT("No element preset is loaded for '%s'."), *Element);
			return Result;
		}

		EffectiveInputs->SetObjectField(
			TEXT("preset_material_parameters"),
			CloneJsonObject(Preset->MaterialParameterOverrides));
		TSharedPtr<FJsonObject> NiagaraParameters = CloneJsonObject(Preset->NiagaraParameters);
		if (NiagaraParameters.IsValid())
		{
			const TSharedPtr<FJsonValue> ScaleOverride = EffectiveInputs->TryGetField(TEXT("scale"));
			if (ScaleOverride.IsValid())
			{
				NiagaraParameters->SetField(TEXT("scale"), CloneJsonValue(ScaleOverride));
			}
			const TSharedPtr<FJsonValue> IntensityOverride = EffectiveInputs->TryGetField(TEXT("intensity"));
			if (IntensityOverride.IsValid())
			{
				NiagaraParameters->SetField(TEXT("intensity"), CloneJsonValue(IntensityOverride));
			}
			EffectiveInputs->SetObjectField(TEXT("preset_niagara_parameters"), NiagaraParameters);
		}
	}

	FString EffectiveTargetPath;
	EffectiveInputs->TryGetStringField(TEXT("target_path"), EffectiveTargetPath);
	if (!EffectiveTargetPath.IsEmpty())
	{
		const FString TargetFolder = FPaths::GetPath(EffectiveTargetPath);
		const FString TargetName = FPaths::GetBaseFilename(EffectiveTargetPath);
		if (!EffectiveInputs->HasField(TEXT("projectile_fx_path")))
		{
			EffectiveInputs->SetStringField(TEXT("projectile_fx_path"), EffectiveTargetPath);
		}
		if (!EffectiveInputs->HasField(TEXT("core_material_path")))
		{
			EffectiveInputs->SetStringField(
				TEXT("core_material_path"),
				FPaths::Combine(TargetFolder, FString::Printf(TEXT("MI_%s_Core"), *TargetName)));
		}
		if (!EffectiveInputs->HasField(TEXT("trail_material_path")))
		{
			EffectiveInputs->SetStringField(
				TEXT("trail_material_path"),
				FPaths::Combine(TargetFolder, FString::Printf(TEXT("MI_%s_Trail"), *TargetName)));
		}
	}

	FString MaterializeError;
	const TSharedPtr<FJsonObject> Plan = MaterializePlan(
		*Record,
		EffectiveInputs,
		Request.Modifiers,
		EffectiveTargetPath,
		Request.Mode,
		Result.ExpectedValidationChecks,
		Result.NonExecutableValidationChecks,
		MaterializeError);
	if (!Plan.IsValid())
	{
		Result.Summary = MaterializeError;
		return Result;
	}

	Result.bSuccess = true;
	Result.Status = TEXT("partially_completed");
	Result.Summary = FString::Printf(
		TEXT("Materialized an execute_plan specification for '%s'."),
		*Request.TemplateId);
	if (const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
		Plan->TryGetArrayField(TEXT("operations"), Operations) && Operations)
	{
		TArray<FString> OperationIds;
		for (const TSharedPtr<FJsonValue>& OperationValue : *Operations)
		{
			FString OperationId;
			if (OperationValue.IsValid()
				&& OperationValue->Type == EJson::Object
				&& OperationValue->AsObject()->TryGetStringField(TEXT("id"), OperationId))
			{
				OperationIds.Add(OperationId);
			}
		}
		Result.InheritedFacts.Add(FString::Printf(
			TEXT("materialized_operations=%s"),
			*FString::Join(OperationIds, TEXT(","))));
	}
	Result.MaterializedPlan = Plan;
	return Result;
}

FUeremcpTemplatePromotionResult FUeremcpTemplateService::PlanPromotion(
	const FUeremcpTemplatePromotionRequest& Request)
{
	FUeremcpTemplatePromotionResult Result;
	if (!Request.SourceAsset.StartsWith(TEXT("/Game/"))
		|| Request.SourceAsset.Contains(TEXT(".."))
		|| Request.SourceAsset.EndsWith(TEXT("/")))
	{
		Result.Summary = TEXT("source_asset must be a non-traversing /Game/ asset path.");
		return Result;
	}

	const FUeremcpTemplateRecord* BaseTemplate = nullptr;
	if (!Request.BaseTemplateId.IsEmpty())
	{
		BaseTemplate = Store.FindById(Request.BaseTemplateId);
		if (!BaseTemplate)
		{
			Result.Summary = FString::Printf(
				TEXT("Unknown base_template_id '%s'."),
				*Request.BaseTemplateId);
			return Result;
		}
	}

	Result.ProposedTemplateId = Request.ProposedTemplateId;
	if (Result.ProposedTemplateId.IsEmpty())
	{
		const FString Domain = BaseTemplate ? BaseTemplate->Domain : TEXT("assets");
		const FString Category = BaseTemplate ? BaseTemplate->Category : TEXT("promoted");
		Result.ProposedTemplateId = FString::Printf(
			TEXT("%s.%s.%s.v1"),
			*Domain.ToLower(),
			*Category.ToLower(),
			*SlugFromAssetPath(Request.SourceAsset));
	}
	if (!IsValidTemplateId(Result.ProposedTemplateId))
	{
		Result.Summary = TEXT("proposed_template_id does not match the versioned template id contract.");
		return Result;
	}

	Result.QuarantinePath = FString::Printf(
		TEXT("/Game/__UeremcpTemplates/agent/%s"),
		*Result.ProposedTemplateId);
	Result.ContractGates = {
		TEXT("template.promotion.source_path_validated"),
		TEXT("template.promotion.template_id_validated"),
		TEXT("template.promotion.quarantine_json_write"),
		TEXT("template.promotion.store_index"),
	};

	TSharedPtr<FJsonObject> Document = MakeShared<FJsonObject>();
	if (BaseTemplate && BaseTemplate->Document.IsValid())
	{
		FString BaseJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BaseJson);
		FJsonSerializer::Serialize(BaseTemplate->Document.ToSharedRef(), Writer);
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BaseJson);
		FJsonSerializer::Deserialize(Reader, Document);
	}
	if (!Document.IsValid())
	{
		Document = MakeShared<FJsonObject>();
	}

	Document->SetStringField(TEXT("template_id"), Result.ProposedTemplateId);
	Document->SetStringField(
		TEXT("domain"),
		BaseTemplate ? BaseTemplate->Domain : TEXT("niagara"));
	Document->SetStringField(
		TEXT("category"),
		BaseTemplate ? BaseTemplate->Category : TEXT("promoted"));
	Document->SetNumberField(TEXT("version"), 1);
	Document->SetStringField(
		TEXT("description"),
		Request.Description.IsEmpty()
			? FString::Printf(TEXT("Promoted from %s"), *Request.SourceAsset)
			: Request.Description);
	Document->SetStringField(TEXT("authored_by"), TEXT("promoted_from_asset"));
	Document->SetStringField(TEXT("promoted_from"), Request.SourceAsset);
	if (!Request.BaseTemplateId.IsEmpty())
	{
		Document->SetStringField(TEXT("inherits_from"), Request.BaseTemplateId);
	}
	if (!Document->HasField(TEXT("construction_plan")))
	{
		TArray<TSharedPtr<FJsonValue>> Plan;
		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("id"), TEXT("promote_source"));
		Op->SetStringField(TEXT("action"), TEXT("no_change_required"));
		TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
		Spec->SetStringField(TEXT("source_asset"), Request.SourceAsset);
		Spec->SetStringField(
			TEXT("note"),
			TEXT("Promotion recorded source identity; expand construction_plan via UpdateTemplate."));
		Op->SetObjectField(TEXT("specification"), Spec);
		Plan.Add(MakeShared<FJsonValueObject>(Op));
		Document->SetArrayField(TEXT("construction_plan"), Plan);
	}

	Result.CapabilityNotes = {
		TEXT("Promotion writes a quarantine JSON template (not a duplicated Unreal asset)."),
		TEXT("Complete Niagara/Blueprint graph re-synthesis from arbitrary assets remains limited — "
			 "construction_plan starts from base template or a source stub; refine with UpdateTemplate."),
	};

	if (!Request.bQuarantine)
	{
		Result.CapabilityNotes.Add(
			TEXT("quarantine=false was not honored: agent writes stay under Saved/UEREMCP/Templates/agent/."));
	}

	const FString AgentDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEREMCP/Templates/agent")));
	const FString FilePath = FPaths::Combine(AgentDir, Result.ProposedTemplateId + TEXT(".json"));
	Result.WrittenFilePath = FilePath;

	if (Request.bDryRun)
	{
		Result.bSuccess = true;
		Result.Status = TEXT("no_change_required");
		Result.WrittenDocument = Document;
		Result.Summary = FString::Printf(
			TEXT("Dry-run promotion of '%s' as '%s' would write %s"),
			*Request.SourceAsset,
			*Result.ProposedTemplateId,
			*FilePath);
		Result.CapabilityNotes.Add(TEXT("dry_run=true — no file written; re-call with options.dry_run=false."));
		return Result;
	}

	FUeremcpTemplateRecord Saved;
	FString SaveError;
	if (!Store.SaveDocument(Document, FilePath, Saved, SaveError))
	{
		Result.Summary = SaveError;
		Result.Status = TEXT("failed_validation");
		Result.CapabilityNotes.Add(TEXT("Quarantine write failed; template was not indexed."));
		return Result;
	}

	Result.bSuccess = true;
	Result.Status = TEXT("created_and_validated");
	Result.WrittenDocument = Document;
	Result.Summary = FString::Printf(
		TEXT("Promoted '%s' to template '%s' at %s; SearchTemplates can find it."),
		*Request.SourceAsset,
		*Result.ProposedTemplateId,
		*FilePath);
	return Result;
}

namespace
{
	FUeremcpTemplateAuthorResult AuthorTemplateInternal(
		FUeremcpTemplateStore& Store,
		const FUeremcpTemplateAuthorRequest& Request,
		const bool bRequireExisting)
	{
		FUeremcpTemplateAuthorResult Result;
		if (!Request.TemplateDocument.IsValid())
		{
			Result.Summary = TEXT("specification.template (object) is required.");
			return Result;
		}

		FString TemplateId;
		if (!Request.TemplateDocument->TryGetStringField(TEXT("template_id"), TemplateId)
			|| TemplateId.IsEmpty())
		{
			Result.Summary = TEXT("template.template_id is required.");
			return Result;
		}
		if (!IsValidTemplateId(TemplateId))
		{
			Result.Summary = TEXT("template_id does not match the versioned template id contract.");
			return Result;
		}

		FString Domain;
		FString Category;
		FString Description;
		Request.TemplateDocument->TryGetStringField(TEXT("domain"), Domain);
		Request.TemplateDocument->TryGetStringField(TEXT("category"), Category);
		Request.TemplateDocument->TryGetStringField(TEXT("description"), Description);
		if (Domain.IsEmpty() || Category.IsEmpty() || Description.IsEmpty())
		{
			Result.Summary = TEXT("template.domain, template.category, and template.description are required.");
			return Result;
		}

		const bool bExists = Store.FindById(TemplateId) != nullptr;
		if (bRequireExisting && !bExists)
		{
			Result.Summary = FString::Printf(
				TEXT("UpdateTemplate: template_id '%s' not found. Use CreateTemplate."),
				*TemplateId);
			return Result;
		}
		if (!bRequireExisting && bExists && !Request.bAllowOverwrite)
		{
			Result.Summary = FString::Printf(
				TEXT("CreateTemplate: template_id '%s' already exists. Pass allow_overwrite=true or use UpdateTemplate."),
				*TemplateId);
			return Result;
		}

		Result.TemplateId = TemplateId;
		FString DestDir = Request.DestinationDirectory;
		if (DestDir.IsEmpty())
		{
			DestDir = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEREMCP/Templates/agent")));
		}
		const FString FilePath = FPaths::Combine(DestDir, TemplateId + TEXT(".json"));
		Result.WrittenFilePath = FilePath;
		Result.WrittenDocument = Request.TemplateDocument;

		if (Request.bDryRun)
		{
			Result.bSuccess = true;
			Result.Status = TEXT("no_change_required");
			Result.Summary = FString::Printf(
				TEXT("Dry-run would %s template '%s' at %s"),
				bRequireExisting ? TEXT("update") : TEXT("create"),
				*TemplateId,
				*FilePath);
			Result.CapabilityNotes.Add(TEXT("dry_run=true — no file written."));
			return Result;
		}

		if (!Request.TemplateDocument->HasField(TEXT("authored_by")))
		{
			Request.TemplateDocument->SetStringField(TEXT("authored_by"), TEXT("agent"));
		}
		if (!Request.TemplateDocument->HasField(TEXT("version")))
		{
			Request.TemplateDocument->SetNumberField(TEXT("version"), 1);
		}

		FUeremcpTemplateRecord Saved;
		FString SaveError;
		if (!Store.SaveDocument(Request.TemplateDocument, FilePath, Saved, SaveError))
		{
			Result.Summary = SaveError;
			return Result;
		}

		Result.bSuccess = true;
		Result.Status = bRequireExisting || bExists
			? TEXT("modified_and_validated")
			: TEXT("created_and_validated");
		Result.Summary = FString::Printf(
			TEXT("%s template '%s' at %s"),
			bRequireExisting || bExists ? TEXT("Updated") : TEXT("Created"),
			*TemplateId,
			*FilePath);
		return Result;
	}
}

FUeremcpTemplateAuthorResult FUeremcpTemplateService::CreateTemplate(
	const FUeremcpTemplateAuthorRequest& Request)
{
	return AuthorTemplateInternal(Store, Request, false);
}

FUeremcpTemplateAuthorResult FUeremcpTemplateService::UpdateTemplate(
	const FUeremcpTemplateAuthorRequest& Request)
{
	return AuthorTemplateInternal(Store, Request, true);
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

bool FUeremcpTemplateService::ValidateInputs(
	const FUeremcpTemplateRecord& Record,
	const TSharedPtr<FJsonObject>& Inputs,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* InputSchema = nullptr;
	if (!Record.Document.IsValid()
		|| !Record.Document->TryGetObjectField(TEXT("inputs"), InputSchema)
		|| !InputSchema
		|| !InputSchema->IsValid())
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
	if ((*InputSchema)->TryGetArrayField(TEXT("required"), Required) && Required)
	{
		for (const TSharedPtr<FJsonValue>& RequiredValue : *Required)
		{
			FString RequiredName;
			if (RequiredValue.IsValid()
				&& RequiredValue->TryGetString(RequiredName)
				&& (!Inputs.IsValid() || !Inputs->HasField(RequiredName)))
			{
				OutError = FString::Printf(TEXT("Missing required template input '%s'."), *RequiredName);
				return false;
			}
		}
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!(*InputSchema)->TryGetObjectField(TEXT("properties"), Properties)
		|| !Properties
		|| !Properties->IsValid()
		|| !Inputs.IsValid())
	{
		return true;
	}

	for (const auto& InputPair : Inputs->Values)
	{
		const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
		if (!(*Properties)->TryGetObjectField(InputPair.Key, PropertySchema)
			|| !PropertySchema
			|| !PropertySchema->IsValid())
		{
			continue;
		}

		FString ExpectedType;
		if ((*PropertySchema)->TryGetStringField(TEXT("type"), ExpectedType))
		{
			const bool bTypeMatches =
				(ExpectedType == TEXT("string") && InputPair.Value->Type == EJson::String)
				|| (ExpectedType == TEXT("number") && InputPair.Value->Type == EJson::Number)
				|| (ExpectedType == TEXT("integer")
					&& InputPair.Value->Type == EJson::Number
					&& InputPair.Value->AsNumber()
						== static_cast<double>(static_cast<int64>(InputPair.Value->AsNumber())))
				|| (ExpectedType == TEXT("boolean") && InputPair.Value->Type == EJson::Boolean)
				|| (ExpectedType == TEXT("object") && InputPair.Value->Type == EJson::Object)
				|| (ExpectedType == TEXT("array") && InputPair.Value->Type == EJson::Array);
			if (!bTypeMatches)
			{
				OutError = FString::Printf(
					TEXT("Template input '%s' must be %s."),
					*InputPair.Key,
					*ExpectedType);
				return false;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* EnumValues = nullptr;
		if ((*PropertySchema)->TryGetArrayField(TEXT("enum"), EnumValues) && EnumValues)
		{
			bool bEnumMatch = false;
			for (const TSharedPtr<FJsonValue>& EnumValue : *EnumValues)
			{
				if (JsonScalarEquals(EnumValue, InputPair.Value))
				{
					bEnumMatch = true;
					break;
				}
			}
			if (!bEnumMatch)
			{
				OutError = FString::Printf(
					TEXT("Template input '%s' is not an allowed value."),
					*InputPair.Key);
				return false;
			}
		}

		if (InputPair.Value->Type == EJson::Number)
		{
			double Bound = 0.0;
			if ((*PropertySchema)->TryGetNumberField(TEXT("minimum"), Bound)
				&& InputPair.Value->AsNumber() < Bound)
			{
				OutError = FString::Printf(
					TEXT("Template input '%s' is below its minimum."),
					*InputPair.Key);
				return false;
			}
			if ((*PropertySchema)->TryGetNumberField(TEXT("maximum"), Bound)
				&& InputPair.Value->AsNumber() > Bound)
			{
				OutError = FString::Printf(
					TEXT("Template input '%s' is above its maximum."),
					*InputPair.Key);
				return false;
			}
		}
	}

	return true;
}

TSharedPtr<FJsonObject> FUeremcpTemplateService::MaterializePlan(
	const FUeremcpTemplateRecord& Record,
	const TSharedPtr<FJsonObject>& Inputs,
	const TSharedPtr<FJsonObject>& Modifiers,
	const FString& TargetAssetPath,
	const FString& Mode,
	TArray<FString>& OutExpectedValidationChecks,
	TArray<FString>& OutNonExecutableValidationChecks,
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

	TArray<TSharedPtr<FJsonValue>> Operations = *MaterializedSteps;
	TArray<TSharedPtr<FJsonValue>> PendingValidationOperations;
	const TSharedPtr<FJsonObject>* ModifierDefinitions = nullptr;
	Record.Document->TryGetObjectField(TEXT("modifier_definitions"), ModifierDefinitions);
	TSet<FString> SeenModifiers;
	static const TCHAR* BucketNames[] = { TEXT("replace"), TEXT("adjust"), TEXT("add"), TEXT("preserve") };
	for (const TCHAR* BucketName : BucketNames)
	{
		const TArray<TSharedPtr<FJsonValue>>* Requested = nullptr;
		if (!Modifiers.IsValid()
			|| !Modifiers->TryGetArrayField(BucketName, Requested)
			|| !Requested)
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& RequestedValue : *Requested)
		{
			FString ModifierName;
			if (!RequestedValue.IsValid() || !RequestedValue->TryGetString(ModifierName))
			{
				OutError = FString::Printf(TEXT("Modifier bucket '%s' contains a non-string value."), BucketName);
				return nullptr;
			}
			if (SeenModifiers.Contains(ModifierName))
			{
				OutError = FString::Printf(TEXT("Modifier '%s' was requested more than once."), *ModifierName);
				return nullptr;
			}
			SeenModifiers.Add(ModifierName);
			if (!Record.SupportedModifiers.Contains(ModifierName))
			{
				OutError = FString::Printf(
					TEXT("Unsupported modifier '%s' for template '%s'."),
					*ModifierName,
					*Record.TemplateId);
				return nullptr;
			}
			const TSharedPtr<FJsonObject>* Definition = nullptr;
			if (!ModifierDefinitions || !ModifierDefinitions->IsValid()
				|| !(*ModifierDefinitions)->TryGetObjectField(ModifierName, Definition)
				|| !Definition
				|| !Definition->IsValid())
			{
				OutError = FString::Printf(
					TEXT("Modifier '%s' is declared but has no executable delta."),
					*ModifierName);
				return nullptr;
			}
			FString DeclaredBucket;
			if (!(*Definition)->TryGetStringField(TEXT("bucket"), DeclaredBucket)
				|| DeclaredBucket != BucketName)
			{
				OutError = FString::Printf(
					TEXT("Modifier '%s' belongs to bucket '%s', not '%s'."),
					*ModifierName,
					*DeclaredBucket,
					BucketName);
				return nullptr;
			}

			const TSharedPtr<FJsonObject>* Replacements = nullptr;
			if ((*Definition)->TryGetObjectField(TEXT("replace_operations"), Replacements)
				&& Replacements
				&& Replacements->IsValid())
			{
				for (const auto& Replacement : (*Replacements)->Values)
				{
					const FString OperationId(Replacement.Key);
					const int32 Index = FindOperation(Operations, OperationId);
					if (Index == INDEX_NONE || !Replacement.Value.IsValid()
						|| Replacement.Value->Type != EJson::Object)
					{
						OutError = FString::Printf(
							TEXT("Modifier '%s' replaces unknown operation '%s'."),
							*ModifierName,
							*OperationId);
						return nullptr;
					}
					const TSharedPtr<FJsonValue> MaterializedReplacement =
						ApplyInputsToJsonValue(Replacement.Value, Inputs);
					FString ReplacementId;
					if (!MaterializedReplacement.IsValid()
						|| MaterializedReplacement->Type != EJson::Object
						|| !MaterializedReplacement->AsObject()->TryGetStringField(TEXT("id"), ReplacementId)
						|| ReplacementId != OperationId)
					{
						OutError = FString::Printf(
							TEXT("Modifier '%s' replacement must preserve operation id '%s'."),
							*ModifierName,
							*OperationId);
						return nullptr;
					}
					Operations[Index] = MaterializedReplacement;
				}
			}

			const TSharedPtr<FJsonObject>* SpecificationPatches = nullptr;
			if ((*Definition)->TryGetObjectField(TEXT("merge_specifications"), SpecificationPatches)
				&& SpecificationPatches
				&& SpecificationPatches->IsValid())
			{
				for (const auto& Patch : (*SpecificationPatches)->Values)
				{
					const FString OperationId(Patch.Key);
					const int32 Index = FindOperation(Operations, OperationId);
					if (Index == INDEX_NONE || !Patch.Value.IsValid() || Patch.Value->Type != EJson::Object)
					{
						OutError = FString::Printf(
							TEXT("Modifier '%s' patches unknown operation '%s'."),
							*ModifierName,
							*OperationId);
						return nullptr;
					}
					const TSharedPtr<FJsonObject> Operation = Operations[Index]->AsObject();
					TSharedPtr<FJsonObject> Specification;
					const TSharedPtr<FJsonObject>* ExistingSpecification = nullptr;
					if (Operation->TryGetObjectField(TEXT("specification"), ExistingSpecification)
						&& ExistingSpecification
						&& ExistingSpecification->IsValid())
					{
						Specification = CloneJsonObject(*ExistingSpecification);
					}
					else
					{
						Specification = MakeShared<FJsonObject>();
					}
					const TSharedPtr<FJsonValue> MaterializedPatch =
						ApplyInputsToJsonValue(Patch.Value, Inputs);
					ApplyMergePatch(Specification, MaterializedPatch->AsObject());
					Operation->SetObjectField(TEXT("specification"), Specification);
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Appends = nullptr;
			if ((*Definition)->TryGetArrayField(TEXT("append_operations"), Appends) && Appends)
			{
				for (const TSharedPtr<FJsonValue>& Append : *Appends)
				{
					Operations.Add(ApplyInputsToJsonValue(Append, Inputs));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* ModifierValidations = nullptr;
			if ((*Definition)->TryGetArrayField(TEXT("validation_operations"), ModifierValidations)
				&& ModifierValidations)
			{
				for (const TSharedPtr<FJsonValue>& Validation : *ModifierValidations)
				{
					PendingValidationOperations.Add(ApplyInputsToJsonValue(Validation, Inputs));
				}
			}
		}
	}

	const int32 ConstructionOperationCount = Operations.Num();
	for (int32 OperationIndex = 0; OperationIndex < ConstructionOperationCount; ++OperationIndex)
	{
		const TSharedPtr<FJsonValue>& OperationValue = Operations[OperationIndex];
		if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object || !Inputs.IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Operation = OperationValue->AsObject();
		FString OperationId;
		FString OperationTargetPath;
		if (Operation->TryGetStringField(TEXT("id"), OperationId)
			&& Inputs->TryGetStringField(OperationId + TEXT("_path"), OperationTargetPath)
			&& !OperationTargetPath.IsEmpty())
		{
			const TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
			Target->SetStringField(TEXT("asset_path"), OperationTargetPath);
			Operation->SetObjectField(TEXT("target"), Target);
		}
	}
	if (ConstructionOperationCount > 0
		&& Operations[ConstructionOperationCount - 1].IsValid()
		&& Operations[ConstructionOperationCount - 1]->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> TerminalOperation =
			Operations[ConstructionOperationCount - 1]->AsObject();
		if (!TargetAssetPath.IsEmpty() && !TerminalOperation->HasField(TEXT("target")))
		{
			const TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
			Target->SetStringField(TEXT("asset_path"), TargetAssetPath);
			TerminalOperation->SetObjectField(TEXT("target"), Target);
			const TSharedPtr<FJsonObject>* TerminalSpecification = nullptr;
			if (TerminalOperation->TryGetObjectField(
					TEXT("specification"),
					TerminalSpecification)
				&& TerminalSpecification
				&& (*TerminalSpecification)->HasField(TEXT("name")))
			{
				(*TerminalSpecification)->SetStringField(
					TEXT("name"),
					FPaths::GetBaseFilename(TargetAssetPath));
			}
		}
		if (!Mode.IsEmpty())
		{
			TerminalOperation->SetStringField(TEXT("mode"), Mode);
		}
	}

	Operations.Append(PendingValidationOperations);
	const TArray<TSharedPtr<FJsonValue>>* ValidationRules = nullptr;
	if (Record.Document->TryGetArrayField(TEXT("validation_rules"), ValidationRules)
		&& ValidationRules)
	{
		for (const TSharedPtr<FJsonValue>& RuleValue : *ValidationRules)
		{
			if (!RuleValue.IsValid() || RuleValue->Type != EJson::Object)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> Rule = RuleValue->AsObject();
			FString RuleId;
			if (!Rule->TryGetStringField(TEXT("rule_id"), RuleId) || RuleId.IsEmpty())
			{
				continue;
			}
			const FString EvidenceId = FString::Printf(
				TEXT("template.%s.%s"),
				*Record.TemplateId,
				*RuleId);
			const TSharedPtr<FJsonObject>* ValidationOperation = nullptr;
			if (Rule->TryGetObjectField(TEXT("operation"), ValidationOperation)
				&& ValidationOperation
				&& ValidationOperation->IsValid())
			{
				Operations.Add(ApplyInputsToJsonValue(
					MakeShared<FJsonValueObject>(*ValidationOperation),
					Inputs));
				OutExpectedValidationChecks.Add(EvidenceId);
			}
			else
			{
				OutNonExecutableValidationChecks.Add(EvidenceId);
			}
		}
	}
	if (!ValidateOperationGraph(Operations, OutError))
	{
		return nullptr;
	}

	const TSharedPtr<FJsonObject> Plan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Transaction = MakeShared<FJsonObject>();
	Transaction->SetBoolField(TEXT("atomic"), true);
	Transaction->SetBoolField(TEXT("rollback_on_failure"), true);
	Transaction->SetStringField(TEXT("compile_policy"), TEXT("at_boundaries"));
	Transaction->SetStringField(TEXT("validate_policy"), TEXT("at_end"));
	Plan->SetObjectField(TEXT("transaction"), Transaction);
	Plan->SetArrayField(TEXT("operations"), Operations);
	Plan->SetStringField(TEXT("on_failure"), TEXT("rollback_all"));
	return Plan;
}
