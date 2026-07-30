// UEREMCP — provisional $ref substitution for batch plans.
//
// Grammar is PROVISIONAL pending WS-02 audit of REAgentTools execute_editor_batch
// and Epic ProgrammaticToolset.execute_tool_script. See docs/proposals/ws-05-batch-grammar-blocked.md.
//
// Schema $comment (schemas/batch/plan.schema.json): any object of the form
//   {"$ref": "<operation_id>.<dotted.path.into.that.operation's.response>"}
// is replaced with the referenced value before the operation executes.
// Resolution failure fails the operation — never substitutes null.
// Owner: WS-05.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEREMCPPROTOCOL_API FUeremcpRefResolve
{
public:
	/**
	 * Walk Specification recursively. Replace every {"$ref":"..."} object with the
	 * resolved value from CompletedResults[operation_id] looking up the dotted path.
	 *
	 * CompletedResults maps operation id -> that operation's response JSON object.
	 *
	 * @return false with OutError on any unresolved or malformed $ref.
	 *         On failure OutSpecification is left unmodified.
	 */
	static bool ResolveInPlace(
		TSharedPtr<FJsonValue>& Specification,
		const TMap<FString, TSharedPtr<FJsonObject>>& CompletedResults,
		FString& OutError);

	/** True when Value is an object whose only (or distinguishing) key is "$ref". */
	static bool IsRefObject(const TSharedPtr<FJsonValue>& Value);

	/** Parse "op.path.to.value" into operation id + remaining path segments. */
	static bool ParseRefString(
		const FString& Ref,
		FString& OutOperationId,
		TArray<FString>& OutPath,
		FString& OutError);

	/** Walk a JSON object along dotted path segments. */
	static TSharedPtr<FJsonValue> LookupPath(
		const TSharedPtr<FJsonObject>& Root,
		const TArray<FString>& Path,
		FString& OutError);
};
