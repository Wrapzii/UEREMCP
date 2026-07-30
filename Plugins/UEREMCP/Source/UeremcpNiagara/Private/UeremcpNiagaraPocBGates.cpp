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

	bool ManifestHasPrimarySystemChange(
		const FUeremcpNiagaraChangeManifestResult& Manifest,
		const FString& SystemPath)
	{
		if (SystemPath.IsEmpty())
		{
			return false;
		}

		for (const FUeremcpAssetRef& Ref : Manifest.CreatedAssets)
		{
			if (Ref.AssetPath == SystemPath)
			{
				return true;
			}
		}
		for (const FUeremcpAssetRef& Ref : Manifest.ModifiedAssets)
		{
			if (Ref.AssetPath == SystemPath)
			{
				return true;
			}
		}
		return false;
	}

	bool ManifestReportsMaterialAssets(const FUeremcpNiagaraChangeManifestResult& Manifest)
	{
		for (const FUeremcpAssetRef& Ref : Manifest.CreatedAssets)
		{
			if (Ref.AssetClass.Contains(TEXT("Material")))
			{
				return true;
			}
		}
		for (const FUeremcpAssetRef& Ref : Manifest.ReusedAssets)
		{
			if (Ref.AssetClass.Contains(TEXT("Material")))
			{
				return true;
			}
		}
		return false;
	}

	bool CreatePipelineCompletedInOnePass(const FUeremcpNiagaraCreateResult& CreateResult)
	{
		return CreateResult.bSuccess
			&& !CreateResult.CreatedAssetPath.IsEmpty()
			&& CreateResult.ChecksPerformed.Contains(TEXT("niagara.create_system_from_template"))
			&& CreateResult.ChecksPerformed.Contains(TEXT("niagara.add_emitters_from_roles"))
			&& CreateResult.ChecksPerformed.Contains(TEXT("niagara.save_package"));
	}
}

FUeremcpNiagaraPocBGateResult FUeremcpNiagaraPocBGates::Evaluate(
	const FUeremcpNiagaraCreateResult& CreateResult,
	const FUeremcpNiagaraRoundTripResult* RoundTrip,
	const FUeremcpNiagaraChangeManifestResult* Manifest)
{
	FUeremcpNiagaraPocBGateResult Out;

	const bool bMaterialsRequested = CreateResult.MaterialBindings.bAttempted
		|| CreateResult.MaterialBindings.InlineMaterialCreates.Num() > 0
		|| CreateResult.MaterialBindings.ResolvedMaterialPaths.Num() > 0;

	if (CreateResult.bSuccess && !CreateResult.CreatedAssetPath.IsEmpty())
	{
		Out.bB1SingleRequestEvaluated = true;
		const bool bMaterialsComplete = !bMaterialsRequested
			|| CreateResult.MaterialBindings.bAllRequestedVerified;
		const bool bEmittersComplete = CreateResult.EmittersAdded.Num() == 0
			|| HasAllPocBEmitterRoles(CreateResult.EmittersAdded);
		const bool bUserParamsComplete = CreateResult.UserVariablesAdded.Num() == 0
			|| HasPocBUserParameters(CreateResult.UserVariablesAdded);
		const bool bCompileComplete = !CreateResult.bCompiled.IsSet() || CreateResult.bCompiled.GetValue();
		Out.bB1SingleRequestComplete = CreatePipelineCompletedInOnePass(CreateResult)
			&& bEmittersComplete
			&& bUserParamsComplete
			&& bMaterialsComplete
			&& bCompileComplete;
		if (Out.bB1SingleRequestComplete)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B1_single_request"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B1_single_request"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B1_single_request"));
	}

	if (CreateResult.bSaved.IsSet())
	{
		Out.bB8AssetsSavedEvaluated = true;
		Out.bB8AssetsSaved = CreateResult.bSaved.GetValue()
			&& CreateResult.ChecksPerformed.Contains(TEXT("niagara.save_package"));
		if (Out.bB8AssetsSaved)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B8_assets_saved"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B8_assets_saved"));
		}
	}
	else
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B8_assets_saved"));
	}
	Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B8_restart_survival"));

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

	if (Manifest && bMaterialsRequested)
	{
		Out.bB2MaterialsManifestEvaluated = true;
		for (const FUeremcpAssetRef& Ref : Manifest->ReusedAssets)
		{
			if (Ref.AssetClass.Contains(TEXT("Material")))
			{
				Out.bB2ReusedAssetsReported = true;
				break;
			}
		}
		for (const FUeremcpAssetRef& Ref : Manifest->CreatedAssets)
		{
			if (Ref.AssetClass.Contains(TEXT("Material")))
			{
				Out.bB2CreatedAssetsReported = true;
				break;
			}
		}
		if (Out.bB2ReusedAssetsReported || Out.bB2CreatedAssetsReported)
		{
			Out.ChecksPerformed.Add(TEXT("niagara.poc_b.B2_material_manifest"));
		}
		else
		{
			Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B2_material_manifest"));
		}
	}
	else if (bMaterialsRequested)
	{
		Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B2_material_manifest"));
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
		const bool bHasSystemChange = ManifestHasPrimarySystemChange(*Manifest, CreateResult.CreatedAssetPath);
		const bool bHasMaterialManifest = !bMaterialsRequested || ManifestReportsMaterialAssets(*Manifest);
		const bool bHasEmitterChanges = CreateResult.EmittersAdded.Num() == 0
			|| Manifest->Changes.Num() >= static_cast<int32>(CreateResult.EmittersAdded.Num());
		Out.bB9ChangeManifestComplete = Out.bB9ChangeManifestPresent
			&& bHasSystemChange
			&& bHasMaterialManifest
			&& bHasEmitterChanges;
		if (Out.bB9ChangeManifestComplete)
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
	Out.ChecksSkipped.Add(TEXT("niagara.poc_b.B10_visible_render"));

	return Out;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(
	const FUeremcpNiagaraPocBGateResult& Result)
{
	TSharedPtr<FJsonObject> Gates = MakeShared<FJsonObject>();
	Gates->SetBoolField(TEXT("round_trip_supported"), false);

	if (Result.bB1SingleRequestEvaluated)
	{
		Gates->SetBoolField(TEXT("B1_single_request_complete"), Result.bB1SingleRequestComplete);
	}
	else
	{
		Gates->SetField(TEXT("B1_single_request_complete"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB3SixEmittersEvaluated)
	{
		Gates->SetBoolField(TEXT("B3_six_emitters_present"), Result.bB3SixEmittersPresent);
	}
	else
	{
		Gates->SetField(TEXT("B3_six_emitters_present"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB2MaterialsManifestEvaluated)
	{
		Gates->SetBoolField(TEXT("B2_reused_assets_reported"), Result.bB2ReusedAssetsReported);
		Gates->SetBoolField(TEXT("B2_created_assets_reported"), Result.bB2CreatedAssetsReported);
	}
	else
	{
		Gates->SetField(TEXT("B2_reused_assets_reported"), MakeShared<FJsonValueNull>());
		Gates->SetField(TEXT("B2_created_assets_reported"), MakeShared<FJsonValueNull>());
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
		Gates->SetBoolField(TEXT("B9_change_manifest_complete"), Result.bB9ChangeManifestComplete);
	}
	else
	{
		Gates->SetField(TEXT("B9_change_manifest_present"), MakeShared<FJsonValueNull>());
		Gates->SetField(TEXT("B9_change_manifest_complete"), MakeShared<FJsonValueNull>());
	}

	if (Result.bB8AssetsSavedEvaluated)
	{
		Gates->SetBoolField(TEXT("B8_assets_saved"), Result.bB8AssetsSaved);
	}
	else
	{
		Gates->SetField(TEXT("B8_assets_saved"), MakeShared<FJsonValueNull>());
	}
	Gates->SetField(TEXT("B8_restart_survival"), MakeShared<FJsonValueNull>());

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

	Gates->SetField(TEXT("B10_visible_render"), MakeShared<FJsonValueNull>());

	TArray<TSharedPtr<FJsonValue>> NeverClaims;
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("created_and_validated")));
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("modified_and_validated")));
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("mcp_transport_one_call")));
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("editor_restart_survival")));
	NeverClaims.Add(MakeShared<FJsonValueString>(TEXT("B10_visible_render_supplementary")));
	Gates->SetArrayField(TEXT("never_claims"), NeverClaims);

	return Gates;
}
