// UEREMCP — goal-level visual verification toolset (WS-11).
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpVisualCaptureToolset.generated.h"

/**
 * Captures an existing Niagara system; it does not author or compile assets.
 * Scene-capture and simulation primitives remain internal so one MCP call returns
 * the complete frame series and machine-readable pixel evidence.
 */
UCLASS()
class UEREMCPVALIDATION_API UUeremcpVisualCaptureToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }

	/**
	 * Build a transient stage in the editor world, deterministically step the
	 * target Niagara system, export PNG frames under Saved/UEREMCP/VfxCapture,
	 * reread every exported file, and remove all spawned actors.
	 *
	 * This verifies that pixels changed against the empty-stage baseline. It does
	 * not judge appearance or prove that the source Niagara asset compiles.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Validation")
	static FString CaptureEffectFrames(const FString& RequestJson);
};
