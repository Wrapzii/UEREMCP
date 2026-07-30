// UEREMCP — Post-create inspect round-trip helpers (WS-07).

#include "UeremcpNiagaraRoundTrip.h"

#include "UeremcpNiagaraGraphHash.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraInspectMapping.h"
#include "UeremcpNiagaraPaths.h"

bool FUeremcpNiagaraRoundTrip::EvaluateStructuralMatch(
	const FUeremcpNiagaraCreateResult& CreateResult,
	const TArray<TSharedPtr<FJsonValue>>& InspectGraphs,
	TArray<FString>& OutMismatches)
{
	OutMismatches.Reset();

	const TSharedPtr<FJsonObject> SystemGraph = FUeremcpNiagaraInspectMapping::FindSystemGraph(InspectGraphs);
	if (!SystemGraph.IsValid())
	{
		OutMismatches.Add(TEXT("inspect graphs missing NiagaraSystemGraph"));
		return false;
	}

	TSet<FString> EmitterGraphNames;
	for (const TSharedPtr<FJsonValue>& GraphValue : InspectGraphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		Graph->TryGetStringField(TEXT("graph_type"), GraphType);
		if (GraphType != TEXT("NiagaraEmitterGraph"))
		{
			continue;
		}
		FString GraphName;
		if (Graph->TryGetStringField(TEXT("graph_name"), GraphName))
		{
			EmitterGraphNames.Add(GraphName);
		}
	}

	const int32 EmitterGraphCount = EmitterGraphNames.Num();
	if (CreateResult.EmittersAdded.Num() > 0 && EmitterGraphCount < CreateResult.EmittersAdded.Num())
	{
		OutMismatches.Add(FString::Printf(
			TEXT("emitter graph count %d < created emitters %d"),
			EmitterGraphCount,
			CreateResult.EmittersAdded.Num()));
	}

	const TArray<FString> InspectedUserVars = FUeremcpNiagaraInspectMapping::ReadUserParameterNames(SystemGraph);

	bool bOk = true;
	for (const FString& Emitter : CreateResult.EmittersAdded)
	{
		if (!EmitterGraphNames.Contains(Emitter))
		{
			OutMismatches.Add(FString::Printf(TEXT("emitter missing after inspect: %s"), *Emitter));
			bOk = false;
		}
	}

	for (const FString& UserVar : CreateResult.UserVariablesAdded)
	{
		if (!InspectedUserVars.Contains(UserVar))
		{
			OutMismatches.Add(FString::Printf(TEXT("user_parameter missing after inspect: %s"), *UserVar));
			bOk = false;
		}
	}

	return bOk && OutMismatches.Num() == 0;
}

bool FUeremcpNiagaraRoundTrip::ValidateCreateResult(
	const FUeremcpRequest& OriginalRequest,
	const FUeremcpNiagaraCreateResult& CreateResult,
	FUeremcpNiagaraRoundTripResult& OutResult)
{
	OutResult = FUeremcpNiagaraRoundTripResult();

	if (CreateResult.CreatedAssetPath.IsEmpty())
	{
		OutResult.Summary = TEXT("Round-trip skipped: no created asset path.");
		OutResult.ChecksSkipped.Add(TEXT("niagara.post_create_inspect"));
		return false;
	}

	if (!UeremcpNiagaraPaths::IsAllowedProbePath(CreateResult.CreatedAssetPath))
	{
		OutResult.Summary = TEXT("Round-trip skipped: asset outside probe root.");
		OutResult.ChecksSkipped.Add(TEXT("niagara.post_create_inspect"));
		return false;
	}

	FUeremcpRequest InspectRequest = OriginalRequest;
	InspectRequest.Action = TEXT("inspect_system");
	InspectRequest.TargetAssetPath = CreateResult.CreatedAssetPath;
	InspectRequest.bDryRun = false;
	InspectRequest.ResponseDetail = TEXT("complete");

	FUeremcpNiagaraInspectSpec InspectSpec;
	FString SpecError;
	if (!FUeremcpNiagaraInspect::ParseSpecification(InspectRequest.Specification, InspectSpec, SpecError))
	{
		InspectSpec = FUeremcpNiagaraInspectSpec();
	}
	InspectSpec.bIncludeInputValues = false;

	FUeremcpNiagaraInspectResult InspectResult;
	if (!FUeremcpNiagaraInspect::Run(InspectRequest, InspectSpec, InspectResult))
	{
		OutResult.Summary = FString::Printf(
			TEXT("Post-create inspect failed: %s"),
			*InspectResult.Error);
		OutResult.ChecksSkipped.Add(TEXT("niagara.post_create_inspect"));
		return false;
	}

	OutResult.bInspectSucceeded = true;
	OutResult.InspectGraphs = InspectResult.Graphs;
	OutResult.InternalOperations = InspectResult.InternalOperations;
	OutResult.ChecksPerformed.Add(TEXT("niagara.post_create_inspect"));

	OutResult.bStructuralMatch = EvaluateStructuralMatch(
		CreateResult,
		InspectResult.Graphs,
		OutResult.Mismatches);

	FUeremcpNiagaraHashRoundTripResult HashScaffold;
	FUeremcpNiagaraHashRoundTrip::RecordPostInspectScaffold(InspectResult.Graphs, HashScaffold);
	for (const FString& Check : HashScaffold.ChecksPerformed)
	{
		OutResult.ChecksPerformed.Add(Check);
	}
	for (const FString& Check : HashScaffold.ChecksSkipped)
	{
		if (!OutResult.ChecksSkipped.Contains(Check))
		{
			OutResult.ChecksSkipped.Add(Check);
		}
	}
	OutResult.HashScaffold = HashScaffold;

	if (OutResult.bStructuralMatch)
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.structural_match"));
		OutResult.Summary = FString::Printf(
			TEXT("Post-create inspect structural match for '%s' (%d emitter graph(s), %d user var(s)). Not hash round-trip validated."),
			*CreateResult.CreatedAssetPath,
			CreateResult.EmittersAdded.Num(),
			CreateResult.UserVariablesAdded.Num());
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.structural_match"));
		OutResult.Summary = FString::Printf(
			TEXT("Post-create inspect for '%s' found mismatches: %s"),
			*CreateResult.CreatedAssetPath,
			*FString::Join(OutResult.Mismatches, TEXT("; ")));
	}

	// Honest: retrieve→replace→retrieve hash stability is not proven here.
	if (!OutResult.ChecksSkipped.Contains(TEXT("niagara.content_hash_round_trip_stability")))
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
	}

	return true;
}
