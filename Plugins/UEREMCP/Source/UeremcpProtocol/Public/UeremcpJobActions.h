// UEREMCP — Core-facing public action adapters for ADR-0009 jobs.
//
// Owner: WS-05. Core exposes these through thin AICallable UFUNCTION wrappers.

#pragma once

#include "CoreMinimal.h"

class UEREMCPPROTOCOL_API FUeremcpJobActions
{
public:
	/** Implements action=get_job_result against the process-wide registry. */
	static FString GetJobResult(const FString& RequestJson);

	/** Implements action=cancel_job with cooperative registry cancellation. */
	static FString CancelJob(const FString& RequestJson);
};
