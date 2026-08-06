// UEREMCP — Niagara domain toolset (WS-07).

#include "UeremcpNiagaraToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpNiagaraAdapt.h"
#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraChangeManifest.h"
#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraCreateIdempotency.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraInspect.h"
#include "UeremcpNiagaraMaterialBindingDiagnostics.h"
#include "UeremcpNiagaraModuleResolve.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraPocBGates.h"
#include "UeremcpNiagaraProbeAssets.h"
#include "UeremcpNiagaraRoundTrip.h"
#include "UeremcpNiagaraSubmit.h"
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

	FUeremcpNiagaraInspectSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraInspect::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid inspect_system specification: %s"), *SpecError));
	}

	// Prefer complete graphs by default (docs/WHY.md). Envelope defaults response_detail=summary.
	bool bExplicitResponseDetail = false;
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* Options = nullptr;
			if (Root->TryGetObjectField(TEXT("options"), Options) && Options && (*Options).IsValid()
				&& (*Options)->HasField(TEXT("response_detail")))
			{
				bExplicitResponseDetail = true;
			}
		}
		if (!Spec.ResponseDetail.IsEmpty())
		{
			Request.ResponseDetail = Spec.ResponseDetail;
			bExplicitResponseDetail = true;
		}
	}
	if (!bExplicitResponseDetail)
	{
		Request.ResponseDetail = TEXT("complete");
	}

	FString ResolvedPath;
	FString ResolveError;
	TArray<FString> ResolveCandidates;
	if (!FUeremcpNiagaraInspect::ResolveTargetPath(
		Request, Spec, ResolvedPath, ResolveError, ResolveCandidates))
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(
			TEXT("target.asset_path"),
			TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"));
		Next->SetStringField(TEXT("specification.query"), TEXT("NS_nature_xl_cast"));
		Next->SetStringField(TEXT("hint"),
			TEXT("Pass target.asset_path or specification.query/asset_name. Prefer UEREMCP InspectSystem over Epic NiagaraToolsets."));
		if (ResolveCandidates.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> CandVals;
			for (const FString& C : ResolveCandidates)
			{
				CandVals.Add(MakeShared<FJsonValueString>(C));
			}
			Next->SetArrayField(TEXT("candidates"), CandVals);
		}
		const bool bPathDenied = !Request.TargetAssetPath.IsEmpty()
			&& !FUeremcpNiagaraInspect::IsAllowedInspectPath(Request.TargetAssetPath);
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			ResolveError,
			bPathDenied ? TEXT("NIAGARA_INSPECT_PATH_DENIED") : TEXT("UNKNOWN"),
			Next);
	}
	Request.TargetAssetPath = ResolvedPath;

	FUeremcpNiagaraInspectResult InspectResult;
	if (!FUeremcpNiagaraInspect::Run(Request, Spec, InspectResult))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			InspectResult.Error.IsEmpty() ? TEXT("inspect_system failed.") : InspectResult.Error);
	}

	const FString PrimaryPath = InspectResult.ResolvedAssetPath.IsEmpty()
		? ResolvedPath
		: InspectResult.ResolvedAssetPath;

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = InspectResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = PrimaryPath;
	// Do NOT set Response.PrimaryAsset: SerializeResponse builds a thin result from it
	// and then skips ExtraFields.result (HasField guard). Agents need result.graphs[].
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultInspectCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = InspectResult.InternalOperations;

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();

	// One-shot agent surface (docs/WHY.md): topology_summary FIRST, then fat graphs when complete.
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("primary_asset"), PrimaryPath);
	ResultObj->SetStringField(TEXT("asset_path"), PrimaryPath);
	ResultObj->SetStringField(
		TEXT("asset_name"),
		UeremcpNiagaraPaths::AssetNameFromAssetPath(PrimaryPath));
	if (InspectResult.TopologySummary.IsValid())
	{
		ResultObj->SetObjectField(TEXT("topology_summary"), InspectResult.TopologySummary);
	}
	ResultObj->SetNumberField(TEXT("emitter_count"), InspectResult.EmitterCount);
	ResultObj->SetNumberField(TEXT("module_count"), InspectResult.ModuleCount);
	ResultObj->SetNumberField(TEXT("renderer_count"), InspectResult.RendererCount);
	{
		TArray<TSharedPtr<FJsonValue>> EmitterNameVals;
		for (const FString& Name : InspectResult.EmitterNames)
		{
			EmitterNameVals.Add(MakeShared<FJsonValueString>(Name));
		}
		ResultObj->SetArrayField(TEXT("emitters"), EmitterNameVals);
	}
	if (InspectResult.UserParameters.Num() > 0)
	{
		ResultObj->SetArrayField(TEXT("user_parameters"), InspectResult.UserParameters);
	}
	TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
	Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
	{
		TArray<TSharedPtr<FJsonValue>> Lossy;
		for (const FString& Area : UeremcpNiagaraCapability::DefaultFidelityLossyAreas())
		{
			Lossy.Add(MakeShared<FJsonValueString>(Area));
		}
		Lossy.Add(MakeShared<FJsonValueString>(
			UeremcpNiagaraCapability::LossyAreaRendererMaterialBindings));
		Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
	}
	ResultObj->SetObjectField(TEXT("fidelity"), Fidelity);

	// summary → topology_summary + counts (no full graphs). complete → + full graphs[].
	// minimal → topology_summary only. Default remains complete (WHY.md).
	const bool bSummaryOnly = Request.ResponseDetail.Equals(TEXT("summary"), ESearchCase::IgnoreCase);
	const bool bMinimal = Request.ResponseDetail.Equals(TEXT("minimal"), ESearchCase::IgnoreCase);
	const bool bIncludeGraphs = !bSummaryOnly && !bMinimal;
	if (bIncludeGraphs && InspectResult.Graphs.Num() > 0)
	{
		ResultObj->SetArrayField(TEXT("graphs"), InspectResult.Graphs);
	}
	ResultObj->SetStringField(
		TEXT("response_detail"),
		Request.ResponseDetail.IsEmpty() ? TEXT("complete") : Request.ResponseDetail);
	Extra->SetObjectField(TEXT("result"), ResultObj);

	// Diagnostics: execution_trace / hash_scaffold only — not the primary graph dump.
	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	if (InspectResult.ExecutionTrace.Num() > 0
		&& (Request.ResponseDetail.Equals(TEXT("diagnostic"), ESearchCase::IgnoreCase)
			|| Request.ResponseDetail.Equals(TEXT("complete"), ESearchCase::IgnoreCase)))
	{
		Diagnostics->SetArrayField(TEXT("execution_trace"), InspectResult.ExecutionTrace);
	}

	FUeremcpNiagaraHashRoundTripResult HashScaffold;
	FUeremcpNiagaraHashRoundTrip::RecordPostInspectScaffold(InspectResult.Graphs, HashScaffold);
	if (HashScaffold.bHashesPresent)
	{
		Diagnostics->SetObjectField(
			TEXT("hash_scaffold"),
			FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(HashScaffold));
	}
	if (Diagnostics->Values.Num() > 0)
	{
		Extra->SetObjectField(TEXT("diagnostics"), Diagnostics);
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
				*UeremcpNiagaraPaths::AllowedMutateRootsDescription()));
	}

	if (!FUeremcpNiagaraInspect::IsAllowedProbePath(Request.TargetAssetPath))
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(
			TEXT("target.asset_path"),
			TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_YourSpell"));
		Next->SetStringField(TEXT("next_action"), TEXT("adapt_niagara_effect"));
		Next->SetStringField(
			TEXT("hint"),
			TEXT("WRITE under /Game/RE/VFX/Magecraft/** (create/adapt) or sandbox. Inspect any /Game path."));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			UeremcpNiagaraPaths::MutateDeniedReason(Request.TargetAssetPath),
			TEXT("NIAGARA_MUTATE_PATH_DENIED"),
			Next);
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
		Validation->SetBoolField(TEXT("reread_after_write"), RoundTripResult.bInspectSucceeded);
	}
	else
	{
		Validation->SetField(TEXT("structurally_valid"), MakeShared<FJsonValueNull>());
		Validation->SetField(TEXT("reread_after_write"), MakeShared<FJsonValueNull>());
	}
	Validation->SetField(TEXT("runtime_smoke_test"), MakeShared<FJsonValueNull>());

	const FUeremcpNiagaraMaterialBindingResult& Materials = CreateResult.MaterialBindings;
	if (Materials.bAllRequestedVerified)
	{
		Validation->SetBoolField(TEXT("material_bindings_verified"), true);
	}
	else
	{
		Validation->SetField(TEXT("material_bindings_verified"), MakeShared<FJsonValueNull>());
	}

	Extra->SetObjectField(TEXT("validation"), Validation);

	{
		TSharedPtr<FJsonObject> Authored = MakeShared<FJsonObject>();
		auto StringArray = [](const TArray<FString>& Items) -> TArray<TSharedPtr<FJsonValue>>
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const FString& Item : Items)
			{
				Out.Add(MakeShared<FJsonValueString>(Item));
			}
			return Out;
		};
		Authored->SetArrayField(TEXT("emitters_added"), StringArray(CreateResult.EmittersAdded));
		Authored->SetArrayField(TEXT("modules_added"), StringArray(CreateResult.ModulesAdded));
		Authored->SetArrayField(TEXT("renderers_added"), StringArray(CreateResult.RenderersAdded));
		if (CreateResult.LossyWarnings.Num() > 0)
		{
			Authored->SetArrayField(TEXT("lossy_warnings"), StringArray(CreateResult.LossyWarnings));
		}
		Authored->SetBoolField(TEXT("round_trip_supported"), false);
		Extra->SetObjectField(TEXT("authored"), Authored);
	}

	if (Materials.ResolvedMaterialPaths.Num() > 0
		|| Materials.UnresolvedMaterialBindings.Num() > 0
		|| Materials.CreatedMaterialAssetsPendingWs08.Num() > 0)
	{
		TSharedPtr<FJsonObject> MaterialDiagnostics = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> Resolved = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : Materials.ResolvedMaterialPaths)
		{
			Resolved->SetStringField(Pair.Key, Pair.Value);
		}
		MaterialDiagnostics->SetObjectField(TEXT("resolved_material_paths"), Resolved);

		auto StringArray = [](const TArray<FString>& Items) -> TArray<TSharedPtr<FJsonValue>>
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const FString& Item : Items)
			{
				Out.Add(MakeShared<FJsonValueString>(Item));
			}
			return Out;
		};

		if (Materials.RendererBindingsApplied.Num() > 0)
		{
			MaterialDiagnostics->SetArrayField(
				TEXT("renderer_bindings_applied"),
				StringArray(Materials.RendererBindingsApplied));
		}
		if (Materials.RendererBindingsVerified.Num() > 0)
		{
			MaterialDiagnostics->SetArrayField(
				TEXT("renderer_bindings_verified"),
				StringArray(Materials.RendererBindingsVerified));
		}
		if (Materials.UnresolvedMaterialBindings.Num() > 0)
		{
			MaterialDiagnostics->SetArrayField(
				TEXT("unresolved_material_bindings"),
				StringArray(Materials.UnresolvedMaterialBindings));
		}
		if (Materials.CreatedMaterialAssetsPendingWs08.Num() > 0)
		{
			MaterialDiagnostics->SetArrayField(
				TEXT("ws08_create_spec_pending"),
				StringArray(Materials.CreatedMaterialAssetsPendingWs08));
		}

		TSharedPtr<FJsonObject> ExtraDiagnostics = Extra->HasField(TEXT("diagnostics"))
			? Extra->GetObjectField(TEXT("diagnostics"))
			: MakeShared<FJsonObject>();
		ExtraDiagnostics->SetObjectField(TEXT("material_bindings"), MaterialDiagnostics);
		Extra->SetObjectField(TEXT("diagnostics"), ExtraDiagnostics);
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

FString UUeremcpNiagaraToolset::AdaptNiagaraEffect(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("adapt_niagara_effect"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("AdaptNiagaraEffect received action '%s'. Use action 'adapt_niagara_effect'."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("adapt_niagara_effect requires target.asset_path under %s."),
				*UeremcpNiagaraPaths::AllowedMutateRootsDescription()));
	}

	if (!UeremcpNiagaraPaths::IsAllowedMutatePath(Request.TargetAssetPath))
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(
			TEXT("target.asset_path"),
			TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"));
		Next->SetStringField(TEXT("hint"),
			TEXT("Adapt writes sandbox or Magecraft; never deletes. Inspect any /Game path first."));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			UeremcpNiagaraPaths::MutateDeniedReason(Request.TargetAssetPath),
			TEXT("NIAGARA_MUTATE_SANDBOX_ONLY"),
			Next);
	}

	FUeremcpNiagaraAdaptSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraAdapt::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid adapt_niagara_effect specification: %s"), *SpecError));
	}

	const double HandlerStartSeconds = FPlatformTime::Seconds();
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			/*bAssetExists=*/true,
			/*PredictedDeleted=*/0,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	FUeremcpNiagaraAdaptResult AdaptResult;
	if (!FUeremcpNiagaraAdapt::Run(Request, Spec, AdaptResult))
	{
		FUeremcpResponse FailResponse;
		FailResponse.RequestId = Request.RequestId;
		FailResponse.Status = TEXT("failed_validation");
		FailResponse.Summary = AdaptResult.Error.IsEmpty()
			? TEXT("adapt_niagara_effect failed.")
			: AdaptResult.Error;
		FailResponse.UnderstoodAction = Request.Action;
		FailResponse.UnderstoodTarget = Request.TargetAssetPath;
		FailResponse.PrimaryAsset = Request.TargetAssetPath;
		FailResponse.CapabilityNotes = UeremcpNiagaraCapability::DefaultAdaptCapabilityNotes();
		FailResponse.Metrics.McpRoundTrips = 1;
		return bDispatchStarted
			? MutatingDispatch.Complete(FailResponse)
			: FUeremcpEnvelope::SerializeResponse(FailResponse);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	// Leave PrimaryAsset empty so ExtraFields.result is not dropped by SerializeResponse.
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultAdaptCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = AdaptResult.InternalOperations;
	Response.Metrics.TimingMs.Add(
		TEXT("server_total"),
		(FPlatformTime::Seconds() - HandlerStartSeconds) * 1000.0);

	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = AdaptResult.Summary;
	}
	else
	{
		Response.Status = (AdaptResult.bCompiled.Get(false) && AdaptResult.bSaved.Get(false))
			? TEXT("modified_and_validated")
			: TEXT("partially_completed");
		Response.Summary = AdaptResult.Summary;
	}

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("primary_asset"), Request.TargetAssetPath);
	ResultObj->SetStringField(TEXT("asset_path"), Request.TargetAssetPath);
	{
		TArray<TSharedPtr<FJsonValue>> Touched;
		for (const FString& Name : AdaptResult.UserVariablesTouched)
		{
			Touched.Add(MakeShared<FJsonValueString>(Name));
		}
		ResultObj->SetArrayField(TEXT("user_variables_touched"), Touched);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Emitters;
		for (const FString& Name : AdaptResult.EmitterNames)
		{
			Emitters.Add(MakeShared<FJsonValueString>(Name));
		}
		ResultObj->SetArrayField(TEXT("emitters"), Emitters);
	}
	Extra->SetObjectField(TEXT("result"), ResultObj);

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ChecksPerformed;
	for (const FString& Check : AdaptResult.ChecksPerformed)
	{
		ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);
	TArray<TSharedPtr<FJsonValue>> ChecksSkipped;
	for (const FString& Check : AdaptResult.ChecksSkipped)
	{
		ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);
	if (AdaptResult.bCompiled.IsSet())
	{
		Validation->SetBoolField(TEXT("compiled"), AdaptResult.bCompiled.GetValue());
	}
	if (AdaptResult.bSaved.IsSet())
	{
		Validation->SetBoolField(TEXT("saved"), AdaptResult.bSaved.GetValue());
	}
	Extra->SetObjectField(TEXT("validation"), Validation);

	if (TSharedPtr<FJsonObject> Materials =
		FUeremcpNiagaraMaterialBindingDiagnostics::BuildMaterialBindingsObject(AdaptResult.MaterialBindings))
	{
		Extra->SetObjectField(TEXT("material_bindings"), Materials);
	}

	Response.ExtraFields = Extra;
	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpNiagaraToolset::SubmitNiagaraGraph(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("submit_niagara_graph"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("SubmitNiagaraGraph received action '%s'. Use action 'submit_niagara_graph'."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("submit_niagara_graph requires target.asset_path under %s."),
				*UeremcpNiagaraPaths::AllowedMutateRootsDescription()));
	}

	if (!UeremcpNiagaraPaths::IsAllowedMutatePath(Request.TargetAssetPath))
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(
			TEXT("target.asset_path"),
			TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"));
		Next->SetStringField(TEXT("hint"),
			TEXT("Submit writes sandbox or Magecraft in-place. Never deletes Magecraft. Inspect first."));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			UeremcpNiagaraPaths::MutateDeniedReason(Request.TargetAssetPath),
			TEXT("NIAGARA_MUTATE_PATH_DENIED"),
			Next);
	}

	FUeremcpNiagaraSubmitSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraSubmit::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid submit_niagara_graph specification: %s"), *SpecError));
	}

	const bool bAssetExists = UeremcpNiagaraProbeAssets::AssetExistsAtPath(Request.TargetAssetPath);
	if (!bAssetExists)
	{
		TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
		Next->SetStringField(TEXT("next_action"), TEXT("create_niagara_effect"));
		Next->SetStringField(TEXT("hint"),
			TEXT("submit_niagara_graph requires an existing system. Create first, or pass inspect graphs after create."));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("submit_niagara_graph target does not exist: '%s'"),
				*Request.TargetAssetPath),
			TEXT("UNKNOWN"),
			Next);
	}

	const double HandlerStartSeconds = FPlatformTime::Seconds();
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			/*bAssetExists=*/true,
			/*PredictedDeleted=*/0,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	FUeremcpNiagaraSubmitResult SubmitResult;
	if (!FUeremcpNiagaraSubmit::Run(Request, Spec, SubmitResult))
	{
		FUeremcpResponse FailResponse;
		FailResponse.RequestId = Request.RequestId;
		FailResponse.Status = TEXT("failed_validation");
		FailResponse.Summary = SubmitResult.Error.IsEmpty()
			? TEXT("submit_niagara_graph failed.")
			: SubmitResult.Error;
		FailResponse.UnderstoodAction = Request.Action;
		FailResponse.UnderstoodTarget = Request.TargetAssetPath;
		FailResponse.PrimaryAsset = Request.TargetAssetPath;
		FailResponse.CapabilityNotes = UeremcpNiagaraCapability::DefaultSubmitCapabilityNotes();
		FailResponse.Metrics.McpRoundTrips = 1;
		return bDispatchStarted
			? MutatingDispatch.Complete(FailResponse)
			: FUeremcpEnvelope::SerializeResponse(FailResponse);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	// Leave PrimaryAsset empty so ExtraFields.result (graphs / planned_changes) survives SerializeResponse.
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultSubmitCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = SubmitResult.InternalOperations;
	Response.Metrics.TimingMs.Add(
		TEXT("server_total"),
		(FPlatformTime::Seconds() - HandlerStartSeconds) * 1000.0);

	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = SubmitResult.Summary;
	}
	else
	{
		// Never claim *_validated for graph submit until content_hash round-trip is proven.
		Response.Status = TEXT("partially_completed");
		Response.Summary = SubmitResult.Summary;
	}

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("primary_asset"), Request.TargetAssetPath);
	ResultObj->SetStringField(TEXT("asset_path"), Request.TargetAssetPath);
	{
		TArray<TSharedPtr<FJsonValue>> Planned;
		for (const FString& Change : SubmitResult.PlannedChanges)
		{
			Planned.Add(MakeShared<FJsonValueString>(Change));
		}
		ResultObj->SetArrayField(TEXT("planned_changes"), Planned);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Added;
		for (const FString& M : SubmitResult.ModulesAdded)
		{
			Added.Add(MakeShared<FJsonValueString>(M));
		}
		ResultObj->SetArrayField(TEXT("modules_added"), Added);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Removed;
		for (const FString& M : SubmitResult.ModulesRemoved)
		{
			Removed.Add(MakeShared<FJsonValueString>(M));
		}
		ResultObj->SetArrayField(TEXT("modules_removed"), Removed);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Enabled;
		for (const FString& M : SubmitResult.ModulesEnabledChanged)
		{
			Enabled.Add(MakeShared<FJsonValueString>(M));
		}
		ResultObj->SetArrayField(TEXT("modules_enabled_changed"), Enabled);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Touched;
		for (const FString& Name : SubmitResult.UserVariablesTouched)
		{
			Touched.Add(MakeShared<FJsonValueString>(Name));
		}
		ResultObj->SetArrayField(TEXT("user_variables_touched"), Touched);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Mats;
		for (const FString& M : SubmitResult.RendererMaterialsApplied)
		{
			Mats.Add(MakeShared<FJsonValueString>(M));
		}
		ResultObj->SetArrayField(TEXT("renderer_materials_applied"), Mats);
	}
	TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
	Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
	{
		TArray<TSharedPtr<FJsonValue>> Lossy;
		for (const FString& Area : UeremcpNiagaraCapability::DefaultFidelityLossyAreas())
		{
			Lossy.Add(MakeShared<FJsonValueString>(Area));
		}
		Lossy.Add(MakeShared<FJsonValueString>(
			UeremcpNiagaraCapability::LossyAreaRendererMaterialBindings));
		Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
	}
	ResultObj->SetObjectField(TEXT("fidelity"), Fidelity);
	if (SubmitResult.PostInspectGraphs.Num() > 0)
	{
		ResultObj->SetArrayField(TEXT("graphs"), SubmitResult.PostInspectGraphs);
	}
	Extra->SetObjectField(TEXT("result"), ResultObj);

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ChecksPerformed;
	for (const FString& Check : SubmitResult.ChecksPerformed)
	{
		ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);
	TArray<TSharedPtr<FJsonValue>> ChecksSkipped;
	for (const FString& Check : SubmitResult.ChecksSkipped)
	{
		ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);
	if (SubmitResult.bCompiled.IsSet())
	{
		Validation->SetBoolField(TEXT("compiled"), SubmitResult.bCompiled.GetValue());
	}
	if (SubmitResult.bSaved.IsSet())
	{
		Validation->SetBoolField(TEXT("saved"), SubmitResult.bSaved.GetValue());
	}
	Validation->SetBoolField(TEXT("structurally_valid"), SubmitResult.bStructuralMatchAfter);
	Extra->SetObjectField(TEXT("validation"), Validation);

	if (SubmitResult.LossyWarnings.Num() > 0)
	{
		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Warnings;
		for (const FString& W : SubmitResult.LossyWarnings)
		{
			Warnings.Add(MakeShared<FJsonValueString>(W));
		}
		Diagnostics->SetArrayField(TEXT("warnings"), Warnings);
		Extra->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	Response.ExtraFields = Extra;
	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}

namespace UeremcpNiagaraCatalog
{
	/** Groups the alias table by module asset so each module appears once with all its ids. */
	struct FModuleEntry
	{
		FString AssetPath;
		TArray<FString> PrimitiveIds;
	};

	static TArray<FModuleEntry> BuildModuleEntries(const FString& Search)
	{
		TMap<FString, FModuleEntry> ByPath;
		for (const TPair<FString, FString>& Alias : UeremcpNiagaraModuleResolve::ModuleAliasTable())
		{
			FModuleEntry& Entry = ByPath.FindOrAdd(Alias.Value);
			Entry.AssetPath = Alias.Value;
			Entry.PrimitiveIds.Add(Alias.Key);
		}

		TArray<FModuleEntry> Entries;
		for (TPair<FString, FModuleEntry>& Pair : ByPath)
		{
			FModuleEntry& Entry = Pair.Value;
			if (!Search.IsEmpty())
			{
				bool bMatches = Entry.AssetPath.Contains(Search, ESearchCase::IgnoreCase);
				for (const FString& Id : Entry.PrimitiveIds)
				{
					bMatches = bMatches || Id.Contains(Search, ESearchCase::IgnoreCase);
				}
				if (!bMatches)
				{
					continue;
				}
			}
			Entry.PrimitiveIds.Sort();
			Entries.Add(Entry);
		}

		Entries.Sort([](const FModuleEntry& A, const FModuleEntry& B)
		{
			return A.AssetPath < B.AssetPath;
		});
		return Entries;
	}

	static TArray<TSharedPtr<FJsonValue>> ToStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Json;
		for (const FString& Value : Values)
		{
			Json.Add(MakeShared<FJsonValueString>(Value));
		}
		return Json;
	}

	/** Stack-input value modes FUeremcpNiagaraStackInputs::TryBuildStackInputValue accepts. */
	static TArray<FString> SupportedInputModes()
	{
		return {
			TEXT("local"),
			TEXT("linked"),
			TEXT("hlsl_expression"),
			TEXT("data_interface"),
			TEXT("dynamic_input"),
			TEXT("enum"),
		};
	}

	static const TArray<FString>& CatalogCapabilityNotes()
	{
		static const TArray<FString> Notes = {
			TEXT("niagara_catalog.primitive_ids_are_authoritative_for_create_and_submit"),
			TEXT("niagara_catalog.this_is_ueremcp_vocabulary_not_a_niagara_module_library_browse"),
			TEXT("niagara_catalog.for_module_input_names_use_epic_GetModuleSchemaFromAsset"),
			TEXT("niagara_catalog.for_full_module_library_use_epic_FindNiagaraScripts"),
			TEXT("niagara_catalog.asset_path_accepted_for_modules_outside_this_table"),
			TEXT("niagara_catalog.custom_hlsl_and_script_graph_authorship_unsupported"),
		};
		return Notes;
	}
}

FString UUeremcpNiagaraToolset::DescribeNiagaraCatalog(const FString& RequestJson)
{
	using namespace UeremcpNiagaraCatalog;

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
	if (!Request.Action.Equals(TEXT("describe_niagara_catalog"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("DescribeNiagaraCatalog received action '%s'; expected 'describe_niagara_catalog'."),
				*Request.Action));
	}

	FString Search;
	bool bVerifyAssets = true;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("search"), Search);
		Request.Specification->TryGetBoolField(TEXT("verify_assets"), bVerifyAssets);
	}

	const TArray<FModuleEntry> Entries = BuildModuleEntries(Search);

	int32 Resolved = 0;
	int32 Unresolved = 0;
	TArray<TSharedPtr<FJsonValue>> ModuleJson;
	for (const FModuleEntry& Entry : Entries)
	{
		TSharedPtr<FJsonObject> Module = MakeShared<FJsonObject>();
		Module->SetStringField(TEXT("asset_path"), Entry.AssetPath);
		Module->SetArrayField(TEXT("primitive_ids"), ToStringArray(Entry.PrimitiveIds));
		Module->SetStringField(
			TEXT("default_script_usage"),
			UeremcpNiagaraModuleResolve::DefaultScriptUsageForModule(Entry.AssetPath));

		if (bVerifyAssets)
		{
			// Loading proves the table row is not stale, and catches a module that
			// moved between engine versions before the agent authors against it.
			FString LoadError;
			const bool bLoaded =
				UeremcpNiagaraModuleResolve::LoadModuleScript(Entry.AssetPath, LoadError) != nullptr;
			Module->SetBoolField(TEXT("resolves"), bLoaded);
			if (!bLoaded)
			{
				Module->SetStringField(TEXT("resolve_error"), LoadError);
				++Unresolved;
			}
			else
			{
				++Resolved;
			}
		}

		ModuleJson.Add(MakeShared<FJsonValueObject>(Module));
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("no_change_required");
	Response.Summary = bVerifyAssets
		? FString::Printf(
			TEXT("UEREMCP Niagara authoring vocabulary: %d module alias(es) (%d resolved, %d unresolved), %d renderer hint(s), %d script usage(s). ")
			TEXT("For a module's input names call Epic GetModuleSchemaFromAsset; for the full module library, Epic FindNiagaraScripts."),
			Entries.Num(),
			Resolved,
			Unresolved,
			UeremcpNiagaraModuleResolve::SupportedRendererHints().Num(),
			UeremcpNiagaraModuleResolve::SupportedScriptUsages().Num())
		: FString::Printf(
			TEXT("UEREMCP Niagara authoring vocabulary: %d module alias(es) (assets not verified), %d renderer hint(s), %d script usage(s). ")
			TEXT("For a module's input names call Epic GetModuleSchemaFromAsset; for the full module library, Epic FindNiagaraScripts."),
			Entries.Num(),
			UeremcpNiagaraModuleResolve::SupportedRendererHints().Num(),
			UeremcpNiagaraModuleResolve::SupportedScriptUsages().Num());
	Response.UnderstoodAction = Request.Action;
	Response.CapabilityNotes = CatalogCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = bVerifyAssets ? Entries.Num() : 0;

	TSharedPtr<FJsonObject> Catalog = MakeShared<FJsonObject>();
	Catalog->SetArrayField(TEXT("modules"), ModuleJson);
	Catalog->SetArrayField(
		TEXT("renderer_hints"),
		ToStringArray(UeremcpNiagaraModuleResolve::SupportedRendererHints()));
	Catalog->SetArrayField(
		TEXT("script_usages"),
		ToStringArray(UeremcpNiagaraModuleResolve::SupportedScriptUsages()));
	Catalog->SetArrayField(TEXT("input_modes"), ToStringArray(SupportedInputModes()));
	Catalog->SetStringField(
		TEXT("emitter_substrate"),
		UeremcpNiagaraModuleResolve::MinimalEmitterTemplatePath());
	Catalog->SetBoolField(TEXT("assets_verified"), bVerifyAssets);

	Response.ExtraFields = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetObjectField(TEXT("niagara_catalog"), Catalog);
	Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("niagara.catalog.alias_table_enumerated")));
	if (bVerifyAssets)
	{
		Checks.Add(MakeShared<FJsonValueString>(TEXT("niagara.catalog.module_scripts_loaded")));
	}
	Validation->SetArrayField(TEXT("checks_performed"), Checks);
	TArray<TSharedPtr<FJsonValue>> Skipped;
	Skipped.Add(MakeShared<FJsonValueString>(TEXT("niagara.catalog.module_input_enumeration")));
	Validation->SetArrayField(TEXT("checks_skipped"), Skipped);
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

	return FUeremcpEnvelope::SerializeResponse(Response);
}
