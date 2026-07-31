// UEREMCP — publish nested ADR-0003 envelope schemas on live MCP describe (BACKLOG 1.2a / 1b.1).
//
// Epic UHT derives inputSchema from UFUNCTION params. Our AICallable surface takes one
// FString RequestJson, so the default schema is `{requestJson: string}` — a typed hole.
// FToolset::GetJsonSchema() is non-virtual; GetJsonSchemaInternal() is the override point
// [VERIFIED: $TR/Public/ToolsetRegistry/Toolset.h]. FFunctionLibraryToolset (private) owns
// the UObject path, so we wrap the registered FToolset and re-publish nested schemas.
//
// Call convention (both accepted by ExecuteToolInternal):
//   1. Nested envelope object as MCP arguments (preferred for discoverability)
//   2. Legacy {"requestJson":"<envelope JSON string>"}

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

/**
 * Wraps an Epic FFunctionLibraryToolset handler and overrides GetJsonSchemaInternal to
 * publish the per-action envelope + specification schema from schemas/domains/**.
 */
class UEREMCPCORE_API FUeremcpSchemaPublishingToolset final : public UE::ToolsetRegistry::FToolset
{
public:
	explicit FUeremcpSchemaPublishingToolset(TSharedPtr<UE::ToolsetRegistry::FToolset> InInner);

	virtual FString GetToolsetName() const override;
	virtual FString GetToolsetVersion() const override;
	virtual FString GetToolsetDescription() const override;
	virtual UClass* GetToolsetClass() const override;

protected:
	virtual TFuture<TValueOrError<FString, FString>> ExecuteToolInternal(
		const FString& ToolName,
		const FString& JsonInput) override;

	virtual FString GetJsonSchemaInternal() const override;

private:
	TSharedPtr<UE::ToolsetRegistry::FToolset> Inner;
};

namespace UeremcpSchemaPublishing
{
	/** Compute the registry name Epic's FFunctionLibraryToolset would use. */
	UEREMCPCORE_API FString MakeToolsetClassName(const UClass* ToolsetClass);

	/**
	 * After RegisterToolsetClass, replace the handler with a schema-publishing wrapper.
	 * Idempotent: re-wrapping an already-wrapped toolset is a no-op.
	 */
	UEREMCPCORE_API bool PublishNestedSchemasForClass(TSubclassOf<UToolsetDefinition> ToolsetClass);

	/** Wrap every currently registered toolset whose name starts with "Ueremcp". */
	UEREMCPCORE_API int32 PublishNestedSchemasForAllUeremcpToolsets();

	/**
	 * Build the nested ADR-0003 request schema for one tool short name (e.g. BuildEnvironment).
	 * Used by DescribeOperation enrichment and unit tests.
	 */
	UEREMCPCORE_API TSharedPtr<FJsonObject> BuildNestedRequestSchemaForTool(const FString& ToolShortName);

	/** Normalize MCP JsonInput to {"requestJson":"<envelope>"} for the UFUNCTION boundary. */
	UEREMCPCORE_API FString NormalizeArgumentsToRequestJson(const FString& JsonInput);
}
