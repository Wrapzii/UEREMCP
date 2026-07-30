// UEREMCP — request/response envelope (ADR-0003).
//
// Owner: WS-05. Authority: schemas/envelope/*.schema.json — if this header and the
// schema disagree, the schema is right and this is a bug.
// Pure logic, no ToolsetRegistry / ModelContextProtocol dependency.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Mandatory on every response. ADR-0003 rule 3. */
struct UEREMCPPROTOCOL_API FUeremcpMetrics
{
	/** MCP calls the agent spent for this result. Normally 1. */
	int32 McpRoundTrips = 0;

	/** Primitive editor operations performed internally. */
	int32 InternalOperations = 0;

	/** Phase timings in milliseconds: planning, asset_creation, compilation, ... */
	TMap<FString, double> TimingMs;

	int32 AssetsAffected = 0;

	/** True when served from the idempotency store with no work done (ADR-0006). */
	bool bReplayed = false;
};

/** One asset reference as in schemas/common/defs.schema.json#/$defs/assetRef. */
struct UEREMCPPROTOCOL_API FUeremcpAssetRef
{
	FString AssetPath;
	FString AssetClass;
	FString Revision;
	FString Role;
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
	FString TargetActorLabel;

	/** create | create_or_update | replace | patch | rebuild_from_specification |
	 *  repair | delete. Defaults to create_or_update. */
	FString Mode = TEXT("create_or_update");

	/** The only domain-extensible field. Left as raw JSON; domain services parse it. */
	TSharedPtr<FJsonObject> Specification;

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

	/** Opaque revision token; may be null/omitted (ADR-0006). */
	FString ExpectedRevision;
	bool bHasExpectedRevision = false;

	FString IdempotencyKey;
};

/** Response envelope. schemas/envelope/response.schema.json. */
struct UEREMCPPROTOCOL_API FUeremcpResponse
{
	FString ProtocolVersion;
	FString RequestId;

	/** Reflects VERIFIED reality, never tool-call completion (AGENTS.md rule 6). */
	FString Status;

	FString Summary;

	FString UnderstoodAction;
	FString UnderstoodTarget;
	FString UnderstoodTemplate;
	TArray<FString> InterpretationNotes;

	FString PrimaryAsset;
	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FUeremcpAssetRef> ModifiedAssets;
	TArray<FUeremcpAssetRef> DeletedAssets;
	TArray<FUeremcpAssetRef> ReusedAssets;
	TArray<FUeremcpAssetRef> Dependencies;
	TArray<FUeremcpAssetRef> UnresolvedDependencies;

	FString Revision;

	FUeremcpMetrics Metrics;

	TArray<FString> CapabilityNotes;

	/** Raw extensions kept for fields not yet modelled (validation, changes, …).
	 *  Prefer typed fields above; this exists so Serialize can round-trip extras
	 *  without inventing envelope fields. */
	TSharedPtr<FJsonObject> ExtraFields;
};

/** Envelope parse / serialise / validate. */
class UEREMCPPROTOCOL_API FUeremcpEnvelope
{
public:
	/** The protocol version this build speaks. */
	static FString ProtocolVersion();

	/** True when Other's MAJOR matches ours. Minor differences are tolerated;
	 *  a major mismatch is rejected, never best-effort parsed (ADR-0003 rule 4). */
	static bool IsProtocolCompatible(const FString& Other);

	/** Parses and validates against the request schema shape.
	 *  @return false with OutError set on any failure. Never throws. */
	static bool ParseRequest(const FString& Json, FUeremcpRequest& OutRequest, FString& OutError);

	/** Serialise a response. Always includes protocol_version and metrics. */
	static FString SerializeResponse(const FUeremcpResponse& Response);

	/** Validate a response object would satisfy required envelope fields. */
	static bool ValidateResponse(const FUeremcpResponse& Response, FString& OutError);

	/** Convenience for the two rejection paths every tool needs. */
	static FString MakeRejection(const FString& RequestId, const FString& Reason);

	/** Convenience for reporting an operation that ran but could not be verified. */
	static FString MakeUnverified(const FString& RequestId, const FString& Summary,
	                              const TArray<FString>& CapabilityNotes);

	/** Allowed mode values from schemas/common/defs.schema.json#/$defs/mode. */
	static bool IsValidMode(const FString& Mode);

	/** Allowed status values from schemas/common/defs.schema.json#/$defs/status. */
	static bool IsValidStatus(const FString& Status);

	/** Allowed response_detail values. */
	static bool IsValidResponseDetail(const FString& Detail);

	/** Allowed on_revision_conflict values. */
	static bool IsValidRevisionConflictPolicy(const FString& Policy);
};
