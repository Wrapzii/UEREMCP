// UEREMCP — Niagara domain toolset (WS-07).

#include "UeremcpNiagaraToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraChangeManifest.h"
#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraCreateIdempotency.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraInspect.h"
#include "UeremcpNiagaraMaterialBindingDiagnostics.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraPocBGates.h"
#include "UeremcpNiagaraProbeAssets.h"
#include "UeremcpNiagaraRoundTrip.h"
#include "UeremcpSecurityDomainAdoption.h"

#include "NiagaraSystem.h"
#include "HAL/PlatformTime.h"
#include "UObject/SoftObjectPath.h"

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
			FString::Printf(
				TEXT("inspect_system probes only assets under %s."),
				*UeremcpNiagaraPaths::AllowedContentRootsDescription()));
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
			FString::Printf(
				TEXT("create_niagara_effect requires target.asset_path under %s."),
				*UeremcpNiagaraPaths::AllowedContentRootsDescription()));
	}

	if (!FUeremcpNiagaraInspect::IsAllowedProbePath(Request.TargetAssetPath))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("create_niagara_effect probes only assets under %s."),
				*UeremcpNiagaraPaths::AllowedContentRootsDescription()));
	}

	FUeremcpNiagaraCreateSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraCreate::ParseSpecification(Request, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid create_niagara_effect specification: %s"), *SpecError));
	}

	const FString CreatedPath =
		UeremcpNiagaraCreateIdempotency::CreatedAssetPathFromRequest(Request.TargetAssetPath, Spec.Name);
	const bool bAssetExists = UeremcpNiagaraProbeAssets::AssetExistsAtPath(CreatedPath);
	const bool bReplaceMode = UeremcpNiagaraProbeAssets::IsReplaceMode(Request.Mode);

	FString CurrentRevision;
	FString RevisionError;
	if (bAssetExists
		&& !UeremcpNiagaraCreateIdempotency::TryComputeAssetRevision(
			CreatedPath, CurrentRevision, RevisionError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			RevisionError.IsEmpty()
				? TEXT("Failed to compute revision for existing Niagara system.")
				: RevisionError);
	}

	// ADR-0006 E4: stale expected_revision rejects before mutation.
	if (Request.bHasExpectedRevision
		&& bAssetExists
		&& !Request.ExpectedRevision.Equals(CurrentRevision, ESearchCase::CaseSensitive)
		&& !UeremcpNiagaraCreateIdempotency::ShouldBypassRevisionConflict(Request.OnRevisionConflict))
	{
		FUeremcpResponse ConflictResponse;
		ConflictResponse.RequestId = Request.RequestId;
		ConflictResponse.Status = TEXT("rejected");
		ConflictResponse.Summary = FString::Printf(
			TEXT("expected_revision '%s' does not match current revision '%s'; no mutation was performed."),
			*Request.ExpectedRevision,
			*CurrentRevision);
		ConflictResponse.UnderstoodAction = Request.Action;
		ConflictResponse.UnderstoodTarget = Request.TargetAssetPath;
		ConflictResponse.PrimaryAsset = CreatedPath;
		ConflictResponse.Revision = CurrentRevision;
		ConflictResponse.CapabilityNotes.Add(TEXT("revision_conflict.no_mutation"));
		ConflictResponse.CapabilityNotes.Append(UeremcpNiagaraCapability::DefaultCreateCapabilityNotes());
		ConflictResponse.Metrics.McpRoundTrips = 1;
		ConflictResponse.Metrics.InternalOperations = 1;

		TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> ChecksPerformed = {
			MakeShared<FJsonValueString>(TEXT("niagara.current_revision_read")),
			MakeShared<FJsonValueString>(TEXT("niagara.expected_revision_compare")),
		};
		Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);
		TArray<TSharedPtr<FJsonValue>> ChecksSkipped = {
			MakeShared<FJsonValueString>(TEXT("niagara.create_or_replace")),
			MakeShared<FJsonValueString>(TEXT("niagara.compile")),
			MakeShared<FJsonValueString>(TEXT("niagara.reread_after_write")),
		};
		Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);
		Extra->SetObjectField(TEXT("validation"), Validation);
		ConflictResponse.ExtraFields = Extra;
		return FUeremcpEnvelope::SerializeResponse(ConflictResponse);
	}

	// ADR-0006 E3: identical repeated create/replace is a no-op when emitters satisfy Spec.
	if (bAssetExists
		&& (bReplaceMode || Request.Mode.IsEmpty())
		&& UeremcpNiagaraCreateIdempotency::ExistingSatisfiesSpec(CreatedPath, Spec))
	{
		FUeremcpResponse NoChangeResponse;
		NoChangeResponse.RequestId = Request.RequestId;
		NoChangeResponse.Status = TEXT("no_change_required");
		NoChangeResponse.Summary = FString::Printf(
			TEXT("Niagara effect '%s' already satisfies the requested component roles at revision %s; no mutation was performed."),
			*CreatedPath,
			*CurrentRevision);
		NoChangeResponse.UnderstoodAction = Request.Action;
		NoChangeResponse.UnderstoodTarget = Request.TargetAssetPath;
		NoChangeResponse.PrimaryAsset = CreatedPath;
		NoChangeResponse.Revision = CurrentRevision;
		NoChangeResponse.CapabilityNotes = UeremcpNiagaraCapability::DefaultCreateCapabilityNotes();
		NoChangeResponse.CapabilityNotes.Add(TEXT("idempotency.repeated_create_no_change"));
		NoChangeResponse.Metrics.McpRoundTrips = 1;
		NoChangeResponse.Metrics.InternalOperations = 1;

		TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> ChecksPerformed = {
			MakeShared<FJsonValueString>(TEXT("niagara.current_revision_read")),
			MakeShared<FJsonValueString>(TEXT("niagara.expected_revision_compare")),
			MakeShared<FJsonValueString>(TEXT("niagara.existing_satisfies_spec")),
		};
		Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);
		TArray<TSharedPtr<FJsonValue>> ChecksSkipped = {
			MakeShared<FJsonValueString>(TEXT("niagara.create_or_replace")),
			MakeShared<FJsonValueString>(TEXT("niagara.compile")),
			MakeShared<FJsonValueString>(TEXT("niagara.reread_after_write")),
		};
		Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);
		Extra->SetObjectField(TEXT("validation"), Validation);
		NoChangeResponse.ExtraFields = Extra;
		return FUeremcpEnvelope::SerializeResponse(NoChangeResponse);
	}

	const double HandlerStartSeconds = FPlatformTime::Seconds();

	const int32 PredictedDeleted = FUeremcpSecurityDomainAdoption::PredictedDeletedForDestructiveReplace(
		bAssetExists,
		bReplaceMode);
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			bAssetExists,
			PredictedDeleted,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	FUeremcpNiagaraCreateResult CreateResult;
	if (!FUeremcpNiagaraCreate::Run(Request, Spec, CreateResult))
	{
		FUeremcpResponse FailResponse;
		FailResponse.RequestId = Request.RequestId;
		FailResponse.Status = TEXT("failed_validation");
		FailResponse.Summary = CreateResult.Error.IsEmpty()
			? TEXT("create_niagara_effect failed.")
			: CreateResult.Error;
		FailResponse.UnderstoodAction = Request.Action;
		FailResponse.UnderstoodTarget = Request.TargetAssetPath;
		FailResponse.CapabilityNotes = UeremcpNiagaraCapability::DefaultCreateCapabilityNotes();
		FailResponse.Metrics.McpRoundTrips = 1;
		return bDispatchStarted
			? MutatingDispatch.Complete(FailResponse)
			: FUeremcpEnvelope::SerializeResponse(FailResponse);
	}

	FUeremcpNiagaraRoundTripResult RoundTripResult;
	bool bRanRoundTrip = false;
	TOptional<double> ValidationTimingMs;
	if (Request.bValidate && !Request.bDryRun)
	{
		const double ValidationStartSeconds = FPlatformTime::Seconds();
		bRanRoundTrip = FUeremcpNiagaraRoundTrip::ValidateCreateResult(
			Request,
			CreateResult,
			RoundTripResult);
		ValidationTimingMs = (FPlatformTime::Seconds() - ValidationStartSeconds) * 1000.0;
	}

	const FUeremcpNiagaraChangeManifestResult ChangeManifest =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, Request.bDryRun);

	TOptional<FUeremcpNiagaraPocBGateResult> PocBGates;
	if (!Request.bDryRun && CreateResult.EmittersAdded.Num() > 0)
	{
		PocBGates = FUeremcpNiagaraPocBGates::Evaluate(
			CreateResult,
			bRanRoundTrip ? &RoundTripResult : nullptr,
			&ChangeManifest);
	}

	if (!Request.bDryRun && !CreateResult.CreatedAssetPath.IsEmpty())
	{
		// Post-create inspect (validate:true) still needs live MeshRendererInfo DIs; release
		// transient referencers before the caller deletes the probe asset.
		if (UNiagaraSystem* CreatedSystem = Cast<UNiagaraSystem>(
			FSoftObjectPath(CreateResult.CreatedAssetPath).TryLoad()))
		{
			UeremcpNiagaraProbeAssets::ReleaseExternalReferences(CreatedSystem);
		}
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = CreateResult.CreatedAssetPath;
	Response.CreatedAssets = ChangeManifest.CreatedAssets;
	Response.ModifiedAssets = ChangeManifest.ModifiedAssets;
	Response.ReusedAssets = ChangeManifest.ReusedAssets;
	if (!CreateResult.InheritedAssetPath.IsEmpty())
	{
		Response.InterpretationNotes.Add(FString::Printf(
			TEXT("inherited:source_system=%s"),
			*CreateResult.InheritedAssetPath));
		Response.InterpretationNotes.Add(FString::Printf(
			TEXT("inherited:emitters=%s"),
			*FString::Join(CreateResult.EmittersInherited, TEXT(","))));
		Response.InterpretationNotes.Add(FString::Printf(
			TEXT("overridden:element=%s"),
			*Spec.Element));
		Response.InterpretationNotes.Add(FString::Printf(
			TEXT("overridden:added_emitters=%s"),
			*FString::Join(CreateResult.EmittersAdded, TEXT(","))));
	}
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultCreateCapabilityNotes();
	if (CreateResult.bMaterialBindingPartialFailure)
	{
		Response.CapabilityNotes.Add(TEXT(
			"Inline probe MIs were saved but renderer bind/re-read failed; orphaned_inline_creates lists roles left on disk under /Game/__UeremcpTests/Materials/. Status stays partially_completed."));
	}
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = CreateResult.InternalOperations;
	Response.Metrics.AssetsAffected = ChangeManifest.AssetsAffected;
	for (const TPair<FString, double>& TimingEntry : CreateResult.TimingMs)
	{
		Response.Metrics.TimingMs.Add(TimingEntry.Key, TimingEntry.Value);
	}
	if (ValidationTimingMs.IsSet())
	{
		Response.Metrics.TimingMs.Add(TEXT("validation"), ValidationTimingMs.GetValue());
	}
	Response.Metrics.TimingMs.Add(
		TEXT("server_total"),
		(FPlatformTime::Seconds() - HandlerStartSeconds) * 1000.0);
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
		const bool bValidated = PocBGates.IsSet()
			&& FUeremcpNiagaraPocBGates::SupportsValidatedCreateStatus(PocBGates.GetValue());
		Response.Status = bValidated
			? (CreateResult.bReplacedExisting
				? TEXT("modified_and_validated")
				: TEXT("created_and_validated"))
			: TEXT("partially_completed");
		Response.Summary = CreateResult.Summary;
		if (bRanRoundTrip && !RoundTripResult.Summary.IsEmpty())
		{
			Response.Summary += FString::Printf(TEXT(" %s"), *RoundTripResult.Summary);
		}
		if (bValidated)
		{
			Response.Summary += TEXT(
				" Saved package, compile state, six emitter roles, user parameters, "
				"renderer/material bindings, structural re-read, and change manifest were verified.");
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
		Validation->SetBoolField(TEXT("reread_after_write"), RoundTripResult.bInspectSucceeded);
	}
	else
	{
		Validation->SetField(TEXT("structurally_valid"), MakeShared<FJsonValueNull>());
		Validation->SetField(TEXT("reread_after_write"), MakeShared<FJsonValueNull>());
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

	if (CreateResult.bMaterialBindingPartialFailure)
	{
		Validation->SetBoolField(TEXT("material_bindings_orphaned_inline_creates"), true);
	}
	else
	{
		Validation->SetField(TEXT("material_bindings_orphaned_inline_creates"), MakeShared<FJsonValueNull>());
	}

	Extra->SetObjectField(TEXT("validation"), Validation);

	if (PocBGates.IsSet())
	{
		const FUeremcpNiagaraPocBGateResult& Gates = PocBGates.GetValue();
		Extra->SetObjectField(
			TEXT("poc_b_gates"),
			FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(Gates));

		if (Gates.bB1SingleRequestEvaluated)
		{
			Validation->SetBoolField(
				TEXT("single_request_pipeline"),
				Gates.bB1SingleRequestComplete);
		}
		else
		{
			Validation->SetField(TEXT("single_request_pipeline"), MakeShared<FJsonValueNull>());
		}

		for (const FString& Check : Gates.ChecksPerformed)
		{
			ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
		}
		for (const FString& Check : Gates.ChecksSkipped)
		{
			if (!ChecksSkipped.ContainsByPredicate([&Check](const TSharedPtr<FJsonValue>& Value) {
				return Value->AsString() == Check;
			}))
			{
				ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
			}
		}
		Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);
		Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);
	}

	if (TSharedPtr<FJsonObject> Materials =
		FUeremcpNiagaraMaterialBindingDiagnostics::BuildMaterialBindingsObject(CreateResult.MaterialBindings))
	{
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

	if (ChangeManifest.Changes.Num() > 0)
	{
		Extra->SetArrayField(TEXT("changes"), ChangeManifest.Changes);
	}

	Response.ExtraFields = Extra;

	if (!Request.bDryRun && !CreateResult.CreatedAssetPath.IsEmpty())
	{
		FString PostRevision;
		FString PostRevisionError;
		if (UeremcpNiagaraCreateIdempotency::TryComputeAssetRevision(
			CreateResult.CreatedAssetPath, PostRevision, PostRevisionError))
		{
			Response.Revision = PostRevision;
		}
		else if (!CurrentRevision.IsEmpty())
		{
			Response.Revision = CurrentRevision;
		}
	}
	else if (!CurrentRevision.IsEmpty())
	{
		Response.Revision = CurrentRevision;
	}

	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}
