// UEREMCP — deterministic content hashing (ADR-0004, ADR-0006).
//
// Authority for semantics: Docs/CONTENT_HASH.md in this module.
// Schema type: schemas/common/defs.schema.json#/$defs/contentHash (WS-01).
// Owner: WS-05.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Compute a content hash over graph (or other) JSON state.
 *
 * Ignores cosmetic churn: node positions, GUID-like fields, pin_id/node_id identity,
 * retrieved_at, and any pre-existing content_hash/revision fields.
 * Sensitive to: pin defaults, connections, node properties, semantic structure.
 *
 * Format: "sha256:" + lowercase hex digest of the canonical UTF-8 bytes.
 */
class UEREMCPPROTOCOL_API FUeremcpContentHash
{
public:
	/** Hash a parsed JSON value (object or array). Returns empty string on failure. */
	static FString HashJsonValue(const TSharedPtr<FJsonValue>& Value, FString* OutError = nullptr);

	/** Hash a JSON object (typical graph payload). */
	static FString HashJsonObject(const TSharedPtr<FJsonObject>& Object, FString* OutError = nullptr);

	/** Hash a JSON text payload. Parses then hashes. */
	static FString HashJsonString(const FString& Json, FString* OutError = nullptr);

	/** Canonicalise a JSON value for hashing (does not hash). Useful for tests/diffs. */
	static TSharedPtr<FJsonValue> CanonicaliseForHash(const TSharedPtr<FJsonValue>& Value);

	/** SHA-256 of raw bytes, returned as "sha256:<hex>". */
	static FString Sha256Prefixed(const TArray<uint8>& Bytes);

	static FString Sha256Prefixed(const FString& Utf8Text);
};
