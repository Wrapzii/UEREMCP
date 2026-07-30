// Editor automation tests for UeremcpNiagara POC B gate scaffolding (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraChangeManifest.h"
#include "UeremcpNiagaraPocBGates.h"
#include "UeremcpNiagaraRoundTrip.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBGatesOfflineTest,
	"UEREMCP.Niagara.Create.PocBGatesOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBGatesOfflineTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraCreateResult CreateResult;
	CreateResult.EmittersAdded = {
		TEXT("Core"),
		TEXT("FlameShell"),
		TEXT("Sparks"),
		TEXT("Smoke"),
		TEXT("RibbonTrail"),
		TEXT("ImpactBurst"),
	};
	CreateResult.MaterialBindings.bAttempted = true;
	CreateResult.MaterialBindings.bAllRequestedVerified = true;
	CreateResult.UserVariablesAdded = {
		TEXT("User.Color"),
		TEXT("User.SecondaryColor"),
		TEXT("User.Scale"),
		TEXT("User.Intensity"),
	};
	CreateResult.bCompiled = true;
	CreateResult.ChecksPerformed.Add(TEXT("niagara.compile_await"));

	FUeremcpNiagaraRoundTripResult RoundTrip;
	RoundTrip.bInspectSucceeded = true;
	RoundTrip.bStructuralMatch = true;

	FUeremcpNiagaraChangeManifestResult Manifest;
	Manifest.bPopulated = true;
	Manifest.Changes.Add(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));

	const FUeremcpNiagaraPocBGateResult Gates =
		FUeremcpNiagaraPocBGates::Evaluate(CreateResult, &RoundTrip, &Manifest);

	TestTrue(TEXT("B3 six emitters"), Gates.bB3SixEmittersPresent);
	TestTrue(TEXT("B4 verified"), Gates.bB4MaterialBindingsVerified);
	TestTrue(TEXT("B5 user parameters"), Gates.bB5UserParametersPresent);
	TestTrue(TEXT("B6 compile awaited"), Gates.bB6CompileAwaited);
	TestTrue(TEXT("B9 change manifest"), Gates.bB9ChangeManifestPresent);
	TestTrue(TEXT("B7 emitters non-empty"), Gates.bB7EmittersNonEmpty);
	TestTrue(TEXT("B7 structural match"), Gates.bB7StructuralMatch);
	TestTrue(TEXT("B7 structural evaluated"), Gates.bB7StructuralMatchEvaluated);
	TestTrue(
		TEXT("B4 check performed"),
		Gates.ChecksPerformed.Contains(TEXT("niagara.poc_b.B4_material_bindings")));
	TestTrue(
		TEXT("B7 structural check performed"),
		Gates.ChecksPerformed.Contains(TEXT("niagara.poc_b.B7_structural_match")));
	TestTrue(
		TEXT("renderers bound verified"),
		Gates.bB7RenderersBoundEvaluated && Gates.bB7RenderersBound);
	TestTrue(
		TEXT("hash round trip still skipped"),
		Gates.ChecksSkipped.Contains(TEXT("niagara.content_hash_round_trip_stability")));

	const TSharedPtr<FJsonObject> Diagnostics = FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(Gates);
	bool bRoundTripSupported = true;
	TestTrue(TEXT("diagnostics object"), Diagnostics.IsValid());
	TestTrue(TEXT("round_trip_supported field"), Diagnostics->TryGetBoolField(TEXT("round_trip_supported"), bRoundTripSupported));
	TestFalse(TEXT("round_trip_supported false"), bRoundTripSupported);

	bool bB3Present = false;
	TestTrue(TEXT("B3 field"), Diagnostics->TryGetBoolField(TEXT("B3_six_emitters_present"), bB3Present));
	TestTrue(TEXT("B3 true in diagnostics"), bB3Present);

	bool bB9Present = false;
	TestTrue(TEXT("B9 field"), Diagnostics->TryGetBoolField(TEXT("B9_change_manifest_present"), bB9Present));
	TestTrue(TEXT("B9 true in diagnostics"), bB9Present);

	bool bB4Verified = false;
	TestTrue(TEXT("B4 field"), Diagnostics->TryGetBoolField(TEXT("B4_material_bindings_verified"), bB4Verified));
	TestTrue(TEXT("B4 true in diagnostics"), bB4Verified);

	const TArray<TSharedPtr<FJsonValue>>* NeverClaims = nullptr;
	TestTrue(TEXT("never_claims array"), Diagnostics->TryGetArrayField(TEXT("never_claims"), NeverClaims));
	TestTrue(TEXT("never_claims populated"), NeverClaims && NeverClaims->Num() == 2);

	FUeremcpNiagaraCreateResult NoRoundTripCreate = CreateResult;
	const FUeremcpNiagaraPocBGateResult NoInspectGates =
		FUeremcpNiagaraPocBGates::Evaluate(NoRoundTripCreate, nullptr);
	TestFalse(TEXT("no inspect structural evaluated"), NoInspectGates.bB7StructuralMatchEvaluated);
	TestFalse(TEXT("no inspect renderers present evaluated"), NoInspectGates.bB7RenderersPresentEvaluated);
	TestFalse(TEXT("no inspect renderers bound evaluated"), NoInspectGates.bB7RenderersBoundEvaluated);

	const TSharedPtr<FJsonObject> NoInspectDiagnostics =
		FUeremcpNiagaraPocBGates::BuildDiagnosticsObject(NoInspectGates);
	const TSharedPtr<FJsonValue> B7Field = NoInspectDiagnostics->TryGetField(TEXT("B7_structural_match"));
	TestTrue(TEXT("B7 null without inspect"), B7Field.IsValid() && B7Field->IsNull());
	const TSharedPtr<FJsonValue> BoundField = NoInspectDiagnostics->TryGetField(TEXT("B7_renderers_bound"));
	TestTrue(TEXT("renderers bound null without inspect"), BoundField.IsValid() && BoundField->IsNull());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
