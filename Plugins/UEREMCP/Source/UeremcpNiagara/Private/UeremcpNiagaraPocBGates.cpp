// UEREMCP — POC B acceptance gate scaffolding (WS-07).

#include "UeremcpNiagaraPocBGates.h"

FUeremcpNiagaraPocBGateResult FUeremcpNiagaraPocBGates::Evaluate(
	const FUeremcpNiagaraCreateResult& CreateResult,
	const FUeremcpNiagaraRoundTripResult* RoundTrip)
{
	FUeremcpNiagaraPocBGateResult Out;

	Out.bB4Attempted = CreateResult.MaterialBindings.bAttempted;
	Out.bB4MaterialBindingsVerified = CreateResult.MaterialBindings.bAllRequestedVerified;
	if (Out.bB4MaterialBindingsVerified)
	{
		Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B4_material_bindings"));
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B4_material_bindings"));
	}

	Out.bB7EmittersNonEmpty = CreateResult.EmittersAdded.Num() > 0;
	if (Out.bB7EmittersNonEmpty)
	{
		Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B7_emitters_non_empty"));
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_emitters_non_empty"));
	}

	if (RoundTrip && RoundTrip->bInspectSucceeded)
	{
		Out.bB7StructuralMatchEvaluated = true;
		Out.bB7StructuralMatch = RoundTrip->bStructuralMatch;
		if (RoundTrip->bStructuralMatch)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B7_structural_match"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_structural_match"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_structural_match"));
	}

	Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_renderers_bound"));
	Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_data_interfaces_complete"));
	Out.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));

	return Out;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(
	const FUeremcpNiagaraPocBGateResult& Result)
{
	TSharedPtr<FJsonObject> Gates = MakeShared<FJsonObject>();
	Gates->SetBoolField(TEXT("round_trip_supported"), false);

	if (Result.bB4Attempted)
	{
		Gates->SetBoolField(TEXT("B4_material_bindings_verified"), Result.bB4MaterialBindingsVerified);
	}
	else
	{
		Gates->SetField(TEXT("B4_material_bindings_verified"), MakeShared<FJsonValueNull>());
	}

	Gates->SetBoolField(TEXT("B7_emitters_non_empty"), Result.bB7EmittersNonEmpty);

	if (Result.bB7StructuralMatchEvaluated)
	{
		Gates->SetBoolField(TEXT("B7_structural_match"), Result.bB7StructuralMatch);
	}
	else
	{
		Gates->SetField(TEXT("B7_structural_match"), MakeShared<FJsonValueNull>());
	}

	Gates->SetField(TEXT("B7_renderers_bound"), MakeShared<FJsonValueNull>());
	Gates->SetField(TEXT("B7_data_interfaces_complete"), MakeShared<FJsonValueNull>());

	TArray<TSharedPtr<FJsonValue>> NeverClaims;
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("created_and_validated")));
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("modified_and_validated")));
	Gates->SetArrayField(TEXT("never_claims"), NeverClaims);

	return Gates;
}
