// UEREMCP — request/response envelope (ADR-0003).
//
// SCAFFOLD — NOT YET COMPILED. See Plugins/UEREMCP/README.md.
// Owner: WS-05. The authority for these shapes is schemas/envelope/*.schema.json —
// if this header and the schema disagree, the schema is right and this is a bug.

#pragma once

#include "CoreMinimal.h"

/** Mandatory on every response. ADR-0003 rule 3. */
struct UEREMCPPROTOCOL_API FUeremcpMetrics
{
	/** MCP calls the agent spent for this result. Normally 1. */
	int32 McpRoundTrips = 0;

	/** Primitive editor operations performed internally. The ratio to McpRoundTrips
	 *  is the value this project delivers — see docs/WHY.md. */
	int32 InternalOperations = 0;

	/** Phase timings in milliseconds: planning, asset_creation, compilation, ... */
	TMap<FString, double> TimingMs;

	int32 AssetsAffected = 0;

	/** True when served from the idempotency store with no work done (ADR-0006). */
	bool bReplayed = false;
};

/** Parsed request envelope. schemas/envelope/request.schema.json. */
struct UEREMCPPROTOCOL_API FUeremcpRequest
{
	FString ProtocolVersion;
	FString RequestId;
	FString Action;

	FString ProjectPath;
	FString EngineVersion;

	FString TargetAssetPath;
	FString TargetObjectPath;
	FString TargetGraphId;

	/** create | create_or_update | replace | patch | rebuild_from_specification |
	 *  repair | delete. Defaults to create_or_update. */
	FString Mode = TEXT("create_or_update");

	/** The only domain-extensible field. Left as raw JSON here; the owning domain
	 *  service parses it against schemas/domains/<domain>/. */
	TSharedPtr<class FJsonObject> Specification;

	// --- options ---
	bool bDryRun = false;
	bool bAtomic = true;
	bool bRollbackOnFailure = true;
	bool bCompile = true;
	bool bValidate = true;
	bool bSave = true;
	FString ResponseDetail = TEXT("summary");
	int32 TimeoutMs = 0;
	FString OnRevisionConflict = TEXT("reject");
	bool bContinueOnError = false;

	FString ExpectedRevision;
	FString IdempotencyKey;
};

/** Response envelope. schemas/envelope/response.schema.json.
 *
 *  Deliberately incomplete: `changes`, `diagnostics`, `conflict`, `job` and the full
 *  `result` arrays are WS-05's to add. This is the minimum the reference toolset needs.
 *  Grow it against the schema, not against convenience. */
struct UEREMCPPROTOCOL_API FUeremcpResponse
{
	FString RequestId;

	/** Reflects VERIFIED reality, never tool-call completion (AGENTS.md rule 6).
	 *  An operation that could not verify returns partially_completed with a reason —
	 *  never a *_validated status. */
	FString Status;

	FString Summary;

	FString UnderstoodAction;
	FString UnderstoodTarget;

	FString PrimaryAsset;
	FString Revision;

	FUeremcpMetrics Metrics;

	/** Limitations that applied to THIS operation. Silence about a limitation is a
	 *  defect, not brevity. */
	TArray<FString> CapabilityNotes;
};

/** Envelope parse / serialise / validate. Pure logic, no editor dependency, so it is
 *  unit-testable outside the editor (RB-14 q10). */
class UEREMCPPROTOCOL_API FUeremcpEnvelope
{
public:
	/** The protocol version this build speaks. */
	static FString ProtocolVersion();

	/** True when Other's MAJOR matches ours. Minor differences are tolerated;
	 *  a major mismatch is rejected, never best-effort parsed (ADR-0003 rule 4). */
	static bool IsProtocolCompatible(const FString& Other);

	/** Parses and validates against the request schema.
	 *  @return false with OutError set on any failure. Never throws, never checks(). */
	static bool ParseRequest(const FString& Json, FUeremcpRequest& OutRequest, FString& OutError);

	static FString SerializeResponse(const FUeremcpResponse& Response);

	/** Convenience for the two rejection paths every tool needs. */
	static FString MakeRejection(const FString& RequestId, const FString& Reason);

	/** Convenience for reporting an operation that ran but could not be verified.
	 *  Use this instead of reaching for a *_validated status when validation was
	 *  skipped, timed out, or is unsupported for the asset type. */
	static FString MakeUnverified(const FString& RequestId, const FString& Summary,
	                              const TArray<FString>& CapabilityNotes);
};
