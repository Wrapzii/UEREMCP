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

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = Result.Status;
	Response.Summary = Result.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.CapabilityNotes = Result.CapabilityNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = Result.bSuccess ? 1 : 0;

	if (Result.MaterializedPlan.IsValid())
	{
		FString PlanJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PlanJson);
		FJsonSerializer::Serialize(Result.MaterializedPlan.ToSharedRef(), Writer);
		Response.Summary = FString::Printf(TEXT("%s Plan: %s"), *Result.Summary, *PlanJson);
	}

	return FUeremcpEnvelope::SerializeResponse(Response);
}
