// UEREMCP — capability discovery and prepared-action service.
//
// The live ToolsetRegistry is authoritative. Prepared actions are handles into
// this process-owned store, not editable asset snapshots and not arbitrary tool
// invocation requests.

#pragma once

#include "CoreMinimal.h"

class UEREMCPCORE_API FUeremcpCapabilityService
{
public:
	/** Implements action=resolve_and_prepare. */
	static FString ResolveAndPrepare(const FString& RequestJson);

	/** Implements action=execute_prepared_action. */
	static FString ExecutePreparedAction(const FString& RequestJson);

	/** Implements action=get_capability_contract. */
	static FString GetCapabilityContract(const FString& RequestJson);

	/** Implements action=search_capabilities. */
	static FString SearchCapabilities(const FString& RequestJson);

	/** Canonical hash of the current live ToolsetRegistry view. */
	static FString ComputeLiveRegistryHash();

	/** Test-only cleanup for the process-local prepared-action store. */
	static void ResetForTests();
};
