// UEREMCP — Niagara inspect → graph.schema.json mapper (WS-07).
//
// Composes UNiagaraExternalEditUtilities (public NiagaraEditor API) — the same surface
// Epic NiagaraToolsets.NiagaraToolset_System wraps internally.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

/** Parsed inspect_system specification (schemas/domains/niagara/inspect_system.schema.json). */
struct FUeremcpNiagaraInspectSpec
{
	TArray<FString> EmitterFilter;
	TSet<FString> StackFilter;
	bool bIncludeInputValues = true;
	bool bIncludeRenderers = true;
	bool bIncludeDependencies = true;
	bool bIncludeCompileState = true;
	bool bIncludeStackIssues = true;

	/** Optional AssetRegistry name / substring query when target.asset_path is empty. */
	FString Query;
	FString AssetName;
	/** Package path root for query search (default /Game). */
	FString SearchRoot = TEXT("/Game");
	/** Optional override for envelope options.response_detail. */
	FString ResponseDetail;
};

/** Result of a read-only Niagara inspection pass. */
struct FUeremcpNiagaraInspectResult
{
	bool bSuccess = false;
	FString Error;

	FString Summary;
	TArray<TSharedPtr<FJsonValue>> Graphs;
	TArray<TSharedPtr<FJsonValue>> ExecutionTrace;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	TOptional<bool> bCompiled;

	int32 InternalOperations = 0;
	int32 EmitterCount = 0;
	int32 ModuleCount = 0;
	int32 RendererCount = 0;

	FString ResolvedAssetPath;
	TArray<FString> EmitterNames;
	TArray<TSharedPtr<FJsonValue>> UserParameters;
	TArray<FString> Candidates;

	/**
	 * Agent-facing topology summary (emitters → modules → renderers) placed at the
	 * top of result so agents need no Python to scan stacks. Always populated.
	 */
	TSharedPtr<FJsonObject> TopologySummary;
};

/** Maps Epic Niagara topology into ADR-0004 graph shapes + extensions.niagara. */
class FUeremcpNiagaraInspect
{
public:
	static bool ParseSpecification(
		const TSharedPtr<FJsonObject>& Spec,
		FUeremcpNiagaraInspectSpec& OutSpec,
		FString& OutError);

	/** Mutate/create/adapt WRITE guard — sandbox + Magecraft (legacy name kept for tests). */
	static bool IsAllowedProbePath(const FString& AssetPath);

	/** READ inspect guard — any /Game/… path. */
	static bool IsAllowedInspectPath(const FString& AssetPath);

	/**
	 * Resolve target.asset_path from an exact soft path and/or specification.query /
	 * asset_name via AssetRegistry (UNiagaraSystem).
	 */
	static bool ResolveTargetPath(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraInspectSpec& Spec,
		FString& OutAssetPath,
		FString& OutError,
		TArray<FString>& OutCandidates);

	/**
	 * Probe inspect must not call GetStackIssues: diagnostics VM builds renderer stack
	 * items that evaluate FNiagaraMeshMaterialOverride::ExplicitMat edit conditions
	 * (bOverrideMaterials on parent UNiagaraMeshRendererProperties) in struct scope.
	 * Sandbox roots only — Magecraft/production may collect stack issues.
	 * [VERIFIED: GetStackIssues → GetDiagnosticsSystemViewModel — NiagaraExternalSystemEditorUtilities.cpp:3367]
	 */
	static bool ShouldSkipStackIssuesForProbe(const FString& AssetPath);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraInspectSpec& Spec,
		FUeremcpNiagaraInspectResult& OutResult);
};
