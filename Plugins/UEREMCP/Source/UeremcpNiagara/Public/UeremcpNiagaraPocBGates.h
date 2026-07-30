// UEREMCP — POC B acceptance gate scaffolding (WS-07).
//
// Surfaces honest B4/B7 partial status — never claims *_validated or full POC B pass.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraRoundTrip.h"

/** Honest POC B gate evaluation for create_niagara_effect responses. */
struct FUeremcpNiagaraPocBGateResult
{
	bool bB4Attempted = false;
	bool bB4MaterialBindingsVerified = false;

	bool bB7EmittersNonEmpty = false;
	bool bB7StructuralMatch = false;
	bool bB7StructuralMatchEvaluated = false;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
};

class FUeremcpNiagaraPocBGates
{
public:
	static FUeremcpNiagaraPocBGateResult Evaluate(
		const FUeremcpNiagaraCreateResult& CreateResult,
		const FUeremcpNiagaraRoundTripResult* RoundTrip);

	static TSharedPtr<FJsonObject> BuildDiagnosticsObject(const FUeremcpNiagaraPocBGateResult& Result);
};
