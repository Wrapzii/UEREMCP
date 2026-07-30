// UEREMCP — POC B acceptance gate scaffolding (WS-07).
//
// Surfaces honest B4/B7 partial status — never claims *_validated or full POC B pass.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpNiagaraChangeManifest.h"
#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraPocBInspectFidelity.h"
#include "UeremcpNiagaraRoundTrip.h"

/** Honest POC B gate evaluation for create_niagara_effect responses. */
struct FUeremcpNiagaraPocBGateResult
{
	bool bB3SixEmittersPresent = false;
	bool bB3SixEmittersEvaluated = false;

	bool bB4Attempted = false;
	bool bB4MaterialBindingsVerified = false;

	bool bB5UserParametersPresent = false;
	bool bB5UserParametersEvaluated = false;

	bool bB6CompileAwaited = false;
	bool bB6CompileAwaitedEvaluated = false;

	bool bB9ChangeManifestPresent = false;
	bool bB9ChangeManifestEvaluated = false;

	bool bB7EmittersNonEmpty = false;
	bool bB7StructuralMatch = false;
	bool bB7StructuralMatchEvaluated = false;

	bool bB7RenderersPresentEvaluated = false;
	bool bB7RenderersPresent = false;

	bool bB7RenderersBoundEvaluated = false;
	bool bB7RenderersBound = false;

	bool bB7DataInterfacesEvaluated = false;
	bool bB7DataInterfacesComplete = false;

	FUeremcpNiagaraPocBInspectSignals InspectSignals;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
};

class FUeremcpNiagaraPocBGates
{
public:
	static FUeremcpNiagaraPocBGateResult Evaluate(
		const FUeremcpNiagaraCreateResult& CreateResult,
		const FUeremcpNiagaraRoundTripResult* RoundTrip,
		const FUeremcpNiagaraChangeManifestResult* Manifest = nullptr);

	static TSharedPtr<FJsonObject> BuildDiagnosticsObject(const FUeremcpNiagaraPocBGateResult& Result);
};
