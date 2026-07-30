// UEREMCP — POC B acceptance gate scaffolding (WS-07).

#include "UeremcpNiagaraPocBGates.h"

#include "UeremcpNiagaraRoleNames.h"

namespace
{
	bool HasAllPocBEmitterRoles(const TArray<FString>& EmittersAdded)
	{
		for (const FString& Role : UeremcpNiagaraRoles::DefaultPocBComponentRoles())
		{
			const FString EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Role);
			if (!EmittersAdded.Contains(EmitterName))
			{
				return false;
			}
		}
		return true;
	}

	bool HasPocBUserParameters(const TArray<FString>& UserVariablesAdded)
	{
		return UserVariablesAdded.Contains(TEXT("User.Scale"))
			&& UserVariablesAdded.Contains(TEXT("User.Intensity"))
			&& UserVariablesAdded.Contains(TEXT("User.Color"));
	}
}

FUeremcpNiagaraPocBGateResult FUeremcpNiagaraPocBGates::Evaluate(
	const FUeremcpNiagaraCreateResult& CreateResult,
	const FUeremcpNiagaraRoundTripResult* RoundTrip,
	const FUeremcpNiagaraChangeManifestResult* Manifest)
{
	FUeremcpNiagaraPocBGateResult Out;

	if (CreateResult.EmittersAdded.Num() > 0)
	{
		Out.bB3SixEmittersEvaluated = true;
		Out.bB3SixEmittersPresent = HasAllPocBEmitterRoles(CreateResult.EmittersAdded);
		if (Out.bB3SixEmittersPresent)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B3_six_emitters"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B3_six_emitters"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B3_six_emitters"));
	}

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

	if (CreateResult.UserVariablesAdded.Num() > 0)
	{
		Out.bB5UserParametersEvaluated = true;
		Out.bB5UserParametersPresent = HasPocBUserParameters(CreateResult.UserVariablesAdded);
		if (Out.bB5UserParametersPresent)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B5_user_parameters"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B5_user_parameters"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B5_user_parameters"));
	}

	if (CreateResult.bCompiled.IsSet())
	{
		Out.bB6CompileAwaitedEvaluated = true;
		Out.bB6CompileAwaited = CreateResult.bCompiled.GetValue()
			&& CreateResult.ChecksPerformed.Contains(TEXT("niagara.compile_await"));
		if (Out.bB6CompileAwaited)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B6_compile_awaited"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B6_compile_awaited"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B6_compile_awaited"));
	}

	if (Manifest)
	{
		Out.bB9ChangeManifestEvaluated = true;
		Out.bB9ChangeManifestPresent = Manifest->bPopulated && Manifest->Changes.Num() > 0;
		if (Out.bB9ChangeManifestPresent)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B9_change_manifest"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B9_change_manifest"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B9_change_manifest"));
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

		Out.InspectSignals = FUeremcpNiagaraPocBInspectFidelity::Evaluate(
			CreateResult.EmittersAdded,
			RoundTrip->InspectGraphs);

		Out.bB7RenderersPresentEvaluated = true;
		Out.bB7RenderersPresent = Out.InspectSignals.EmittersMissingRenderers.Num() == 0
			&& Out.InspectSignals.TotalRendererRefs > 0
			&& Out.InspectSignals.EmittersWithRendererRefs == Out.InspectSignals.ExpectedEmitterCount;
		if (Out.bB7RenderersPresent)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B7_renderers_present"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_renderers_present"));
		}

		Out.bB7RenderersBoundEvaluated = true;
		Out.bB7RenderersBound = CreateResult.MaterialBindings.bAllRequestedVerified;
		if (Out.bB7RenderersBound)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B7_renderers_bound"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_renderers_bound"));
		}

		if (Out.InspectSignals.bDependenciesPresent)
		{
			Out.bB7DataInterfacesEvaluated = true;
			Out.bB7DataInterfacesComplete = false;
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_data_interfaces_complete"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_data_interfaces_complete"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_structural_match"));
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_renderers_present"));
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_renderers_bound"));
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B7_data_interfaces_complete"));
	}

	Out.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));

	return Out;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(
	const FUeremcpNiagaraPocBGateResult& Result)
{
	TSharedPtr<FJsonObject> Gates = MakeShared<FJsonObject>();
	Gates->SetBoolField(TEXT("round_trip_supported"), false);

	if (Result.bB3SixEmittersEvaluated)
	{
		Gates->SetBoolField(TEXT("B3_six_emitters_present"), Result.bB3SixEmittersPresent);
	}
	else
	{
		Gates->SetField(TEXT("B3_six_emitters_present"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB4Attempted)
	{
		Gates->SetBoolField(TEXT("B4_material_bindings_verified"), Result.bB4MaterialBindingsVerified);
	}
	else
	{
		Gates->SetField(TEXT("B4_material_bindings_verified"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB5UserParametersEvaluated)
	{
		Gates->SetBoolField(TEXT("B5_user_parameters_present"), Result.bB5UserParametersPresent);
	}
	else
	{
		Gates->SetField(TEXT("B5_user_parameters_present"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB6CompileAwaitedEvaluated)
	{
		Gates->SetBoolField(TEXT("B6_compile_awaited"), Result.bB6CompileAwaited);
	}
	else
	{
		Gates->SetField(TEXT("B6_compile_awaited"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB9ChangeManifestEvaluated)
	{
		Gates->SetBoolField(TEXT("B9_change_manifest_present"), Result.bB9ChangeManifestPresent);
	}
	else
	{
		Gates->SetField(TEXT("B9_change_manifest_present"), MakeShared<FJsonValueNull>());
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

	if (Result.bB7RenderersPresentEvaluated)
	{
		Gates->SetBoolField(TEXT("B7_renderers_present"), Result.bB7RenderersPresent);
	}
	else
	{
		Gates->SetField(TEXT("B7_renderers_present"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB7RenderersBoundEvaluated)
	{
		Gates->SetBoolField(TEXT("B7_renderers_bound"), Result.bB7RenderersBound);
	}
	else
	{
		Gates->SetField(TEXT("B7_renderers_bound"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB7DataInterfacesEvaluated)
	{
		Gates->SetBoolField(TEXT("B7_data_interfaces_complete"), Result.bB7DataInterfacesComplete);
	}
	else
	{
		Gates->SetField(TEXT("B7_data_interfaces_complete"), MakeShared<FJsonValueNull>());
	}

	if (Result.InspectSignals.bEvaluated)
	{
		TSharedPtr<FJsonObject> InspectFidelity = MakeShared<FJsonObject>();
		InspectFidelity->SetNumberField(
			TEXT("emitters_with_renderer_refs"),
			Result.InspectSignals.EmittersWithRendererRefs);
		InspectFidelity->SetNumberField(TEXT("total_renderer_refs"), Result.InspectSignals.TotalRendererRefs);
		InspectFidelity->SetNumberField(
			TEXT("renderers_with_extracted_material_path"),
			Result.InspectSignals.RenderersWithExtractedMaterialPath);
		if (Result.InspectSignals.bDependenciesPresent)
		{
			InspectFidelity->SetNumberField(
				TEXT("used_data_interfaces"),
				Result.InspectSignals.UsedDataInterfaces);
		}
		if (Result.InspectSignals.bCompileStatePresent)
		{
			InspectFidelity->SetBoolField(TEXT("compile_has_errors"), Result.InspectSignals.bCompileHasErrors);
		}
		if (Result.InspectSignals.EmittersMissingRenderers.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Missing;
			for (const FString& Emitter : Result.InspectSignals.EmittersMissingRenderers)
			{
				Missing.Add(MakeShared<FJsonValueString>(Emitter));
			}
			InspectFidelity->SetArrayField(TEXT("emitters_missing_renderers"), Missing);
		}
		if (Result.InspectSignals.FidelityNotes.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Notes;
			for (const FString& Note : Result.InspectSignals.FidelityNotes)
			{
				Notes.Add(MakeShared<FJsonValueString>(Note));
			}
			InspectFidelity->SetArrayField(TEXT("notes"), Notes);
		}
		Gates->SetObjectField(TEXT("inspect_fidelity"), InspectFidelity);
	}

	TArray<TSharedPtr<FJsonValue>> NeverClaims;
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("created_and_validated")));
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("modified_and_validated")));
	Gates->SetArrayField(TEXT("never_claims"), NeverClaims);

	return Gates;
}
