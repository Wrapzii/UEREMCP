// UEREMCP — template toolset implementation. Owner: WS-15.

#include "UeremcpTemplatesToolset.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "UeremcpEnvelope.h"
#include "UeremcpTemplateService.h"
#include "UeremcpTemplatesModule.h"

namespace
{
	TSharedPtr<FJsonObject> ParseSpecification(const FUeremcpRequest& Request)
	{
		return Request.Specification;
	}

	bool SerializeJsonObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson)
	{
		if (!Object.IsValid())
		{
			return false;
		}
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	}

	bool BuildExecutePlanRequest(
		const FString& OriginalRequestJson,
		const TSharedPtr<FJsonObject>& Plan,
		FString& OutRequestJson)
	{
		TSharedPtr<FJsonObject> RequestObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OriginalRequestJson);
		if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
		{
			return false;
		}

		RequestObject->SetStringField(TEXT("action"), TEXT("execute_plan"));
		RequestObject->SetObjectField(TEXT("specification"), Plan);
		return SerializeJsonObject(RequestObject, OutRequestJson);
	}

	bool ResolvePromotionDryRun(const FString& RequestJson)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return true;
		}
		const TSharedPtr<FJsonObject>* Options = nullptr;
		bool bDryRun = true;
		if (Root->TryGetObjectField(TEXT("options"), Options)
			&& Options
			&& Options->IsValid())
		{
			(*Options)->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}
		return bDryRun;
	}

	void AppendStringArrayValue(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Field,
		const FString& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
		if (Object->TryGetArrayField(Field, Existing) && Existing)
		{
			Values = *Existing;
		}
		Values.Add(MakeShared<FJsonValueString>(Value));
		Object->SetArrayField(Field, Values);
	}

	bool FinalizeDelegatedResponse(
		const FString& DelegatedResponseJson,
		const FUeremcpRequest& OriginalRequest,
		const FUeremcpTemplateInstantiateRequest& InstantiateRequest,
		const TArray<FString>& ExpectedValidationChecks,
		const TArray<FString>& NonExecutableValidationChecks,
		FString& OutResponseJson,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> ResponseObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DelegatedResponseJson);
		if (!FJsonSerializer::Deserialize(Reader, ResponseObject) || !ResponseObject.IsValid())
		{
			OutError = TEXT("execute_plan returned invalid JSON.");
			return false;
		}

		FString Status;
		FString Summary;
		const TSharedPtr<FJsonObject>* Metrics = nullptr;
		double McpRoundTrips = 0.0;
		double InternalOperations = 0.0;
		if (!ResponseObject->TryGetStringField(TEXT("status"), Status)
			|| !FUeremcpEnvelope::IsValidStatus(Status)
			|| !ResponseObject->TryGetStringField(TEXT("summary"), Summary)
			|| !ResponseObject->TryGetObjectField(TEXT("metrics"), Metrics)
			|| !Metrics
			|| !Metrics->IsValid()
			|| !(*Metrics)->TryGetNumberField(TEXT("mcp_round_trips"), McpRoundTrips)
			|| !(*Metrics)->TryGetNumberField(TEXT("internal_operations"), InternalOperations))
		{
			OutError = TEXT("execute_plan response is missing required structured envelope fields.");
			return false;
		}

		(*Metrics)->SetNumberField(TEXT("mcp_round_trips"), 1);
		ResponseObject->SetStringField(TEXT("request_id"), OriginalRequest.RequestId);
		ResponseObject->SetStringField(
			TEXT("summary"),
			FString::Printf(
				TEXT("Instantiated template '%s' through execute_plan. %s"),
				*InstantiateRequest.TemplateId,
				*Summary));

		TSharedPtr<FJsonObject> Understood;
		const TSharedPtr<FJsonObject>* ExistingUnderstood = nullptr;
		if (ResponseObject->TryGetObjectField(TEXT("understood"), ExistingUnderstood)
			&& ExistingUnderstood
			&& ExistingUnderstood->IsValid())
		{
			Understood = *ExistingUnderstood;
		}
		else
		{
			Understood = MakeShared<FJsonObject>();
		}
		Understood->SetStringField(TEXT("action"), TEXT("instantiate_template"));
		Understood->SetStringField(TEXT("template_used"), InstantiateRequest.TemplateId);
		if (!InstantiateRequest.TargetAssetPath.IsEmpty())
		{
			Understood->SetStringField(TEXT("resolved_target"), InstantiateRequest.TargetAssetPath);
		}
		ResponseObject->SetObjectField(TEXT("understood"), Understood);

		TSharedPtr<FJsonObject> Validation;
		const TSharedPtr<FJsonObject>* ExistingValidation = nullptr;
		if (ResponseObject->TryGetObjectField(TEXT("validation"), ExistingValidation)
			&& ExistingValidation
			&& ExistingValidation->IsValid())
		{
			Validation = *ExistingValidation;
		}
		else
		{
			Validation = MakeShared<FJsonObject>();
		}
		TSet<FString> PerformedChecks;
		const TArray<TSharedPtr<FJsonValue>>* PerformedValues = nullptr;
		if (Validation->TryGetArrayField(TEXT("checks_performed"), PerformedValues)
			&& PerformedValues)
		{
			for (const TSharedPtr<FJsonValue>& Value : *PerformedValues)
			{
				FString Check;
				if (Value.IsValid() && Value->TryGetString(Check))
				{
					PerformedChecks.Add(Check);
				}
			}
		}
		TArray<FString> MissingChecks = NonExecutableValidationChecks;
		for (const FString& Expected : ExpectedValidationChecks)
		{
			if (!PerformedChecks.Contains(Expected))
			{
				MissingChecks.Add(Expected);
			}
		}
		if (MissingChecks.Num() > 0)
		{
			const bool bWouldOtherwiseClaimSuccess =
				Status == TEXT("created_and_validated")
				|| Status == TEXT("modified_and_validated")
				|| Status == TEXT("created_with_warnings")
				|| Status == TEXT("no_change_required");
			if (bWouldOtherwiseClaimSuccess)
			{
				ResponseObject->SetStringField(TEXT("status"), TEXT("partially_completed"));
			}
			AppendStringArrayValue(
				ResponseObject,
				TEXT("capability_notes"),
				TEXT("One or more template validation rules lacked re-read evidence; validated status was withheld."));
			for (const FString& Missing : MissingChecks)
			{
				AppendStringArrayValue(Validation, TEXT("checks_skipped"), Missing);
			}
		}
		ResponseObject->SetObjectField(TEXT("validation"), Validation);

		return SerializeJsonObject(ResponseObject, OutResponseJson);
	}

	FString SerializeSearchResults(
		const FUeremcpRequest& Request,
		const TArray<FUeremcpTemplateSearchHit>& Hits)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(TEXT("Found %d template(s)."), Hits.Num());
		Response.UnderstoodAction = Request.Action;
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = Hits.Num();

		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> HitArray;
		for (const FUeremcpTemplateSearchHit& Hit : Hits)
		{
			const TSharedRef<FJsonObject> HitObject = MakeShared<FJsonObject>();
			HitObject->SetStringField(TEXT("template_id"), Hit.TemplateId);
			HitObject->SetStringField(TEXT("domain"), Hit.Domain);
			HitObject->SetStringField(TEXT("category"), Hit.Category);
			HitObject->SetStringField(TEXT("description"), Hit.Description);
			HitObject->SetNumberField(TEXT("score"), Hit.Score);
			HitArray.Add(MakeShared<FJsonValueObject>(HitObject));
		}
		Result->SetArrayField(TEXT("hits"), HitArray);

		FString ResultJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
		FJsonSerializer::Serialize(Result, Writer);

		// FUeremcpEnvelope::SerializeResponse does not yet carry arbitrary result blobs;
		// embed in summary for v1 discoverability until response schema grows result fields.
		Response.Summary = FString::Printf(
			TEXT("Found %d template(s): %s"),
			Hits.Num(),
			*ResultJson);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}
}

FString UUeremcpTemplatesToolset::SearchTemplates(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), ParseError);
	}

	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("Unsupported protocol_version for template search."));
	}

	const TSharedPtr<FJsonObject> Spec = ParseSpecification(Request);
	FUeremcpTemplateSearchQuery Query;
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("query"), Query.Query);
		Spec->TryGetStringField(TEXT("domain"), Query.Domain);
		Spec->TryGetStringField(TEXT("element"), Query.Element);
		double Limit = Query.Limit;
		if (Spec->TryGetNumberField(TEXT("limit"), Limit))
		{
			Query.Limit = static_cast<int32>(Limit);
		}
	}

	const TArray<FUeremcpTemplateSearchHit> Hits = UeremcpTemplates::GetService().Search(Query);
	return SerializeSearchResults(Request, Hits);
}

FString UUeremcpTemplatesToolset::InstantiateTemplate(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), ParseError);
	}

	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("Unsupported protocol_version for template instantiate."));
	}

	const TSharedPtr<FJsonObject> Spec = ParseSpecification(Request);
	FUeremcpTemplateInstantiateRequest InstantiateRequest;
	if (!Spec.IsValid() || !Spec->TryGetStringField(TEXT("template_id"), InstantiateRequest.TemplateId))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("specification.template_id is required."));
	}

	const TSharedPtr<FJsonObject>* Inputs = nullptr;
	if (Spec->TryGetObjectField(TEXT("inputs"), Inputs))
	{
		InstantiateRequest.Inputs = *Inputs;
	}

	const TSharedPtr<FJsonObject>* Modifiers = nullptr;
	if (Spec->TryGetObjectField(TEXT("modifiers"), Modifiers))
	{
		InstantiateRequest.Modifiers = *Modifiers;
	}

	const TSharedPtr<FJsonObject>* Target = nullptr;
	if (Spec->TryGetObjectField(TEXT("target"), Target) && Target && Target->IsValid())
	{
		(*Target)->TryGetStringField(TEXT("asset_path"), InstantiateRequest.TargetAssetPath);
		(*Target)->TryGetStringField(TEXT("name"), InstantiateRequest.TargetName);
	}

	Spec->TryGetStringField(TEXT("mode"), InstantiateRequest.Mode);

	const FUeremcpTemplateInstantiateResult Result =
		UeremcpTemplates::GetService().Instantiate(InstantiateRequest);

	if (!Result.bSuccess || !Result.MaterializedPlan.IsValid())
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.Status = Result.Status;
		Response.Summary = Result.Summary;
		Response.UnderstoodAction = Request.Action;
		Response.UnderstoodTemplate = InstantiateRequest.TemplateId;
		Response.CapabilityNotes = Result.CapabilityNotes;
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = 0;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString ExecutePlanRequestJson;
	if (!BuildExecutePlanRequest(RequestJson, Result.MaterializedPlan, ExecutePlanRequestJson))
	{
		return FUeremcpEnvelope::MakeUnverified(
			Request.RequestId,
			TEXT("Materialized the template, but could not serialize the execute_plan request."),
			{ TEXT("No asset operation was executed.") });
	}

	FString DelegatedResponseJson;
	FString DelegateError;
	if (!UeremcpTemplates::ExecutePlan(ExecutePlanRequestJson, DelegatedResponseJson, DelegateError))
	{
		return FUeremcpEnvelope::MakeUnverified(
			Request.RequestId,
			FString::Printf(
				TEXT("Materialized template '%s', but execute_plan delegation was unavailable."),
				*InstantiateRequest.TemplateId),
			{ DelegateError, TEXT("No asset operation was executed.") });
	}

	FString FinalResponseJson;
	FString FinalizeError;
	if (!FinalizeDelegatedResponse(
		DelegatedResponseJson,
		Request,
		InstantiateRequest,
		Result.ExpectedValidationChecks,
		Result.NonExecutableValidationChecks,
		FinalResponseJson,
		FinalizeError))
	{
		return FUeremcpEnvelope::MakeUnverified(
			Request.RequestId,
			TEXT("execute_plan returned, but its response could not be validated."),
			{ FinalizeError });
	}

	return FinalResponseJson;
}

FString UUeremcpTemplatesToolset::PromoteToTemplate(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), ParseError);
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("Unsupported protocol_version for template promotion."));
	}
	if (!Request.Action.Equals(TEXT("promote_to_template"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("PromoteToTemplate requires action 'promote_to_template'."));
	}

	const TSharedPtr<FJsonObject> Spec = ParseSpecification(Request);
	FUeremcpTemplatePromotionRequest PromotionRequest;
	if (!Spec.IsValid() || !Spec->TryGetStringField(TEXT("source_asset"), PromotionRequest.SourceAsset))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("specification.source_asset is required."));
	}
	Spec->TryGetStringField(TEXT("base_template_id"), PromotionRequest.BaseTemplateId);
	Spec->TryGetStringField(TEXT("proposed_template_id"), PromotionRequest.ProposedTemplateId);
	Spec->TryGetStringField(TEXT("description"), PromotionRequest.Description);
	Spec->TryGetBoolField(TEXT("quarantine"), PromotionRequest.bQuarantine);
	PromotionRequest.bDryRun = ResolvePromotionDryRun(RequestJson);

	const FUeremcpTemplatePromotionResult Result =
		UeremcpTemplates::GetService().PlanPromotion(PromotionRequest);
	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = Result.Status;
	Response.Summary = Result.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Result.QuarantinePath;
	Response.UnderstoodTemplate = Result.ProposedTemplateId;
	Response.CapabilityNotes = Result.CapabilityNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	if (Result.bSuccess)
	{
		Response.InterpretationNotes.Add(
			PromotionRequest.bDryRun
				? TEXT("Promotion defaulted to preview-only dry_run behavior.")
				: TEXT("Promotion mutation was requested but withheld behind contract gates."));
		Response.InterpretationNotes.Add(
			PromotionRequest.bQuarantine
				? TEXT("Resolved output to the agent quarantine.")
				: TEXT("Resolved a quarantine preview despite quarantine=false."));

		Response.ExtraFields = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Skipped;
		for (const FString& Gate : Result.ContractGates)
		{
			Skipped.Add(MakeShared<FJsonValueString>(Gate));
		}
		Validation->SetArrayField(TEXT("checks_skipped"), Skipped);
		Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);
	}
	return FUeremcpEnvelope::SerializeResponse(Response);
}
