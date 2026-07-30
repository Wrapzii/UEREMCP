// UEREMCP — Niagara domain toolset (WS-07).

#include "UeremcpNiagaraToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraInspect.h"
#include "UeremcpNiagaraRoundTrip.h"

FString UUeremcpNiagaraToolset::Echo(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;

	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}

	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion()));
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("Niagara toolset echoed request for action '%s'. No editor state was touched."),
		*Request.Action);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpNiagaraToolset::InspectSystem(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;

	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}

	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion()));
	}

	if (!Request.Action.Equals(TEXT("inspect_system"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("inspect_system tool received action '%s'. Use action 'inspect_system' or call Echo for protocol checks."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("inspect_system requires target.asset_path (Niagara system package path, e.g. /Game/__UeremcpTests/NS_WS07_Probe)."));
	}

	if (!FUeremcpNiagaraInspect::IsAllowedProbePath(Request.TargetAssetPath))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("inspect_system probes only assets under /Game/__UeremcpTests/."));
	}

	FUeremcpNiagaraInspectSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraInspect::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid inspect_system specification: %s"), *SpecError));
	}

	FUeremcpNiagaraInspectResult InspectResult;
	if (!FUeremcpNiagaraInspect::Run(Request, Spec, InspectResult))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			InspectResult.Error.IsEmpty() ? TEXT("inspect_system failed.") : InspectResult.Error);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = InspectResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultInspectCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = InspectResult.InternalOperations;

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	const bool bIncludeGraphs = !Request.ResponseDetail.Equals(TEXT("minimal"), ESearchCase::IgnoreCase);
	if (bIncludeGraphs && InspectResult.Graphs.Num() > 0)
	{
		Diagnostics->SetArrayField(TEXT("graphs"), InspectResult.Graphs);
	}
	if (InspectResult.ExecutionTrace.Num() > 0
		&& (Request.ResponseDetail.Equals(TEXT("diagnostic"), ESearchCase::IgnoreCase)
			|| Request.ResponseDetail.Equals(TEXT("complete"), ESearchCase::IgnoreCase)))
	{
		Diagnostics->SetArrayField(TEXT("execution_trace"), InspectResult.ExecutionTrace);
	}
	if (Diagnostics->Values.Num() > 0)
	{
		Extra->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	FUeremcpNiagaraHashRoundTripResult HashScaffold;
	FUeremcpNiagaraHashRoundTrip::RecordPostInspectScaffold(InspectResult.Graphs, HashScaffold);
	if (HashScaffold.bHashesPresent)
	{
		TSharedPtr<FJsonObject> ExtraDiagnostics = Extra->HasField(TEXT("diagnostics"))
			? Extra->GetObjectField(TEXT("diagnostics"))
			: MakeShared<FJsonObject>();
		ExtraDiagnostics->SetObjectField(
			TEXT("hash_scaffold"),
			FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(HashScaffold));
		Extra->SetObjectField(TEXT("diagnostics"), ExtraDiagnostics);
	}

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ChecksPerformed;
	for (const FString& Check : InspectResult.ChecksPerformed)
	{
		ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);

	TArray<TSharedPtr<FJsonValue>> ChecksSkipped;
	for (const FString& Check : InspectResult.ChecksSkipped)
	{
		ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);

	if (InspectResult.bCompiled.IsSet())
	{
		Validation->SetBoolField(TEXT("compiled"), InspectResult.bCompiled.GetValue());
	}
	else
	{
		Validation->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
	}
	Extra->SetObjectField(TEXT("validation"), Validation);

	Response.ExtraFields = Extra;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpNiagaraToolset::CreateNiagaraEffect(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;

	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}

	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion()));
	}

	if (!Request.Action.Equals(TEXT("create_niagara_effect"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("CreateNiagaraEffect received action '%s'. Use action 'create_niagara_effect'."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_niagara_effect requires target.asset_path under /Game/__UeremcpTests/."));
	}

	if (!FUeremcpNiagaraInspect::IsAllowedProbePath(Request.TargetAssetPath))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_niagara_effect probes only assets under /Game/__UeremcpTests/."));
	}

	FUeremcpNiagaraCreateSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraCreate::ParseSpecification(Request, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid create_niagara_effect specification: %s"), *SpecError));
	}

	FUeremcpNiagaraCreateResult CreateResult;
	if (!FUeremcpNiagaraCreate::Run(Request, Spec, CreateResult))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			CreateResult.Error.IsEmpty() ? TEXT("create_niagara_effect failed.") : CreateResult.Error);
	}

	FUeremcpNiagaraRoundTripResult RoundTripResult;
	const bool bRanRoundTrip = Request.bValidate
		&& !Request.bDryRun
		&& FUeremcpNiagaraRoundTrip::ValidateCreateResult(Request, CreateResult, RoundTripResult);

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = CreateResult.CreatedAssetPath;
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultCreateCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = CreateResult.InternalOperations;
	if (bRanRoundTrip)
	{
		Response.Metrics.InternalOperations += RoundTripResult.InternalOperations;
	}

	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = CreateResult.Summary;
	}
	else
	{
		Response.Status = TEXT("partially_completed");
		Response.Summary = CreateResult.Summary;
		if (bRanRoundTrip && !RoundTripResult.Summary.IsEmpty())
		{
			Response.Summary += FString::Printf(TEXT(" %s"), *RoundTripResult.Summary);
		}
	}

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> ChecksPerformed;
	for (const FString& Check : CreateResult.ChecksPerformed)
	{
		ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
	}
	if (bRanRoundTrip)
	{
		for (const FString& Check : RoundTripResult.ChecksPerformed)
		{
			ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
		}
	}
	Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);

	TArray<TSharedPtr<FJsonValue>> ChecksSkipped;
	for (const FString& Check : CreateResult.ChecksSkipped)
	{
		ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
	}
	if (bRanRoundTrip)
	{
		for (const FString& Check : RoundTripResult.ChecksSkipped)
		{
			ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
		}
	}
	Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);

	if (CreateResult.bCompiled.IsSet())
	{
		Validation->SetBoolField(TEXT("compiled"), CreateResult.bCompiled.GetValue());
	}
	else
	{
		Validation->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
	}

	if (CreateResult.bSaved.IsSet())
	{
		Validation->SetBoolField(TEXT("saved"), CreateResult.bSaved.GetValue());
	}
	else
	{
		Validation->SetField(TEXT("saved"), MakeShared<FJsonValueNull>());
	}

	if (!Request.bDryRun && CreateResult.bReplacedExisting)
	{
		Validation->SetBoolField(TEXT("replaced_existing"), true);
	}
	else
	{
		Validation->SetField(TEXT("replaced_existing"), MakeShared<FJsonValueNull>());
	}

	if (bRanRoundTrip && RoundTripResult.bInspectSucceeded)
	{
		Validation->SetBoolField(TEXT("structurally_valid"), RoundTripResult.bStructuralMatch);
	}
	else
	{
		Validation->SetField(TEXT("structurally_valid"), MakeShared<FJsonValueNull>());
	}
	Validation->SetField(TEXT("runtime_smoke_test"), MakeShared<FJsonValueNull>());

	if (CreateResult.MaterialBindings.bAttempted || CreateResult.MaterialBindings.UnresolvedMaterialBindings.Num() > 0)
	{
		Validation->SetBoolField(
			TEXT("material_bindings_verified"),
			CreateResult.MaterialBindings.bAllRequestedVerified);
	}
	else
	{
		Validation->SetField(TEXT("material_bindings_verified"), MakeShared<FJsonValueNull>());
	}

	Extra->SetObjectField(TEXT("validation"), Validation);

	if (CreateResult.MaterialBindings.ResolvedMaterialPaths.Num() > 0
		|| CreateResult.MaterialBindings.UnresolvedMaterialBindings.Num() > 0
		|| CreateResult.MaterialBindings.InlineMaterialCreates.Num() > 0)
	{
		TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Resolved = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : CreateResult.MaterialBindings.ResolvedMaterialPaths)
		{
			Resolved->SetStringField(Pair.Key, Pair.Value);
		}
		Materials->SetObjectField(TEXT("resolved_paths"), Resolved);

		auto StringArray = [](const TArray<FString>& Items) {
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FString& Item : Items)
			{
				Values.Add(MakeShared<FJsonValueString>(Item));
			}
			return Values;
		};

		if (CreateResult.MaterialBindings.RendererBindingsApplied.Num() > 0)
		{
			Materials->SetArrayField(
				TEXT("renderer_bindings_applied"),
				StringArray(CreateResult.MaterialBindings.RendererBindingsApplied));
		}
		if (CreateResult.MaterialBindings.RendererBindingsVerified.Num() > 0)
		{
			Materials->SetArrayField(
				TEXT("renderer_bindings_verified"),
				StringArray(CreateResult.MaterialBindings.RendererBindingsVerified));
		}
		if (CreateResult.MaterialBindings.UnresolvedMaterialBindings.Num() > 0)
		{
			Materials->SetArrayField(
				TEXT("unresolved"),
				StringArray(CreateResult.MaterialBindings.UnresolvedMaterialBindings));
		}
		if (CreateResult.MaterialBindings.InlineMaterialCreates.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> InlineValues;
			for (const FUeremcpNiagaraInlineMaterialCreate& Inline : CreateResult.MaterialBindings.InlineMaterialCreates)
			{
				TSharedPtr<FJsonObject> InlineObj = MakeShared<FJsonObject>();
				InlineObj->SetStringField(TEXT("role"), Inline.Role);
				InlineObj->SetBoolField(TEXT("success"), Inline.bSuccess);
				if (!Inline.Status.IsEmpty())
				{
					InlineObj->SetStringField(TEXT("status"), Inline.Status);
				}
				if (!Inline.Summary.IsEmpty())
				{
					InlineObj->SetStringField(TEXT("summary"), Inline.Summary);
				}
				if (!Inline.PrimaryAsset.IsEmpty())
				{
					InlineObj->SetStringField(TEXT("primary_asset"), Inline.PrimaryAsset);
				}
				if (Inline.CapabilityNotes.Num() > 0)
				{
					InlineObj->SetArrayField(TEXT("capability_notes"), StringArray(Inline.CapabilityNotes));
				}
				if (Inline.CreatedAssets.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> CreatedValues;
					for (const FUeremcpAssetRef& Asset : Inline.CreatedAssets)
					{
						TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
						if (!Asset.AssetPath.IsEmpty())
						{
							AssetObj->SetStringField(TEXT("asset_path"), Asset.AssetPath);
						}
						if (!Asset.AssetClass.IsEmpty())
						{
							AssetObj->SetStringField(TEXT("asset_class"), Asset.AssetClass);
						}
						CreatedValues.Add(MakeShared<FJsonValueObject>(AssetObj));
					}
					InlineObj->SetArrayField(TEXT("created_assets"), CreatedValues);
				}
				InlineValues.Add(MakeShared<FJsonValueObject>(InlineObj));
			}
			Materials->SetArrayField(TEXT("inline_material_creates"), InlineValues);
		}

		Extra->SetObjectField(TEXT("material_bindings"), Materials);
	}

	if (bRanRoundTrip && RoundTripResult.InspectGraphs.Num() > 0)
	{
		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetArrayField(TEXT("post_create_inspect_graphs"), RoundTripResult.InspectGraphs);
		if (RoundTripResult.Mismatches.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> MismatchValues;
			for (const FString& Mismatch : RoundTripResult.Mismatches)
			{
				MismatchValues.Add(MakeShared<FJsonValueString>(Mismatch));
			}
			Diagnostics->SetArrayField(TEXT("structural_mismatches"), MismatchValues);
		}
		Extra->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	if (bRanRoundTrip && RoundTripResult.HashScaffold.bHashesPresent)
	{
		TSharedPtr<FJsonObject> ExtraDiagnostics = Extra->HasField(TEXT("diagnostics"))
			? Extra->GetObjectField(TEXT("diagnostics"))
			: MakeShared<FJsonObject>();
		ExtraDiagnostics->SetObjectField(
			TEXT("hash_scaffold"),
			FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(RoundTripResult.HashScaffold));
		Extra->SetObjectField(TEXT("diagnostics"), ExtraDiagnostics);
	}

	if (!Request.bDryRun && CreateResult.EmittersAdded.Num() > 0)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("primary_asset"), CreateResult.CreatedAssetPath);
		Extra->SetObjectField(TEXT("result"), Result);
	}

	Response.ExtraFields = Extra;
	return FUeremcpEnvelope::SerializeResponse(Response);
}
