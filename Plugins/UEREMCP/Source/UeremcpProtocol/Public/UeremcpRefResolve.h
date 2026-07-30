// UEREMCP — batch $ref substitution (final grammar).
//
// Two accepted forms (docs/proposals/ws-05-batch-ref-grammar.md):
//   1. Object: {"$ref": "<operation_id>.<dotted.path>"}  — canonical
//   2. Dollar-string: "$<operation_id>"                   — REAgentTools prior art
//      [VERIFIED: REAgentTools/.../batch_workflow_tools.py:37-48]
//
// Resolution failure fails the operation — never substitutes null.
// Owner: WS-05.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEREMCPPROTOCOL_API FUeremcpRefResolve
{
public:
	/**
	 * Walk Specification recursively. Replace:
	 *   - {"$ref":"op.path..."} objects
	 *   - "$op_id" dollar-strings
	 * with values from CompletedResults.
	 *
	 * CompletedResults maps operation id -> that operation's response JSON object
	 * (or a REAgentTools-shaped step bag with label/path).
	 *
	 * @return false with OutError on any unresolved or malformed $ref.
	 */
	static bool ResolveInPlace(
		TSharedPtr<FJsonValue>& Specification,
		const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
		FString& OutError);

	/** True when Value is {"$ref": "..."} with no other keys. */
	static bool IsRefObject(const TSharedPtr<FJsonValue>& Value);

	/** True when Value is a string matching ^\$[a-zA-Z0-9_-]+$. */
	static bool IsDollarStringRef(const TSharedPtr<FJsonValue>& Value);

	/** Parse object-form "op.path.to.value" into operation id + path segments. */
	static bool ParseObjectRefPath(
		const FString& Ref,
		FString& OutOperationId,
		TArray<FString>& OutPath,
		FString& OutError);

	/**
	 * Resolve "$op_id" shorthand: result.primary_asset → label → path.
	 * Returns a string JsonValue, or null with OutError set.
	 */
	static TSharedPtr<FJsonValue> ResolveDollarShorthand(
		const FString& OperationId,
		const TSharedPtr<FJsonObject>& Completed,
		FString& OutError);

	/** Walk a JSON object along dotted path segments. */
	static TSharedPtr<FJsonValue> LookupPath(
		const TSharedPtr<FJsonObject>& Root,
		const TArray<FString>& Path,
		FString& OutError);

	/** @deprecated Use ParseObjectRefPath. Kept as alias for call sites. */
	static bool ParseRefString(
		const FString& Ref,
		FString& OutOperationId,
		TArray<FString>& OutPath,
		FString& OutError)
	{
		return ParseObjectRefPath(Ref, OutOperationId, OutPath, OutError);
	}
};
