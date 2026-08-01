// UEREMCP — next-step suggestions served with the result (see .cpp for why).
#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class UEREMCPCORE_API FUeremcpNextActions
{
public:
	/**
	 * What to call after CompletedAction, with PrimaryAsset already substituted
	 * into each suggested request. Empty on a terminal failure status, and empty
	 * when the catalog declares no chain for the action.
	 */
	static TArray<TSharedPtr<FJsonObject>> Suggest(
		const FString& CompletedAction,
		const FString& PrimaryAsset,
		const FString& Status);
};
