#include "UeremcpBlueprintMutatingGate.h"

bool FUeremcpBlueprintMutatingGate::TryBeginRead(
	const FString& RequestJson,
	FString& OutBlockingResponseJson)
{
	OutBlockingResponseJson.Reset();
	bActive = false;
	bEffectiveDryRun = false;

#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH
	Dispatch.Emplace();
	bActive = Dispatch->TryBegin(
		RequestJson,
		true,
		0,
		true,
		OutBlockingResponseJson);
	if (bActive)
	{
		bEffectiveDryRun = Dispatch->IsEffectiveDryRun();
	}
	return bActive;
#else
	(void)RequestJson;
	return true;
#endif
}

bool FUeremcpBlueprintMutatingGate::TryBeginMutating(
	const FString& RequestJson,
	const bool bTargetExists,
	FString& OutBlockingResponseJson)
{
	OutBlockingResponseJson.Reset();
	bActive = false;
	bEffectiveDryRun = false;

#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH
	Dispatch.Emplace();
	bActive = Dispatch->TryBegin(
		RequestJson,
		bTargetExists,
		0,
		false,
		OutBlockingResponseJson);
	if (bActive)
	{
		bEffectiveDryRun = Dispatch->IsEffectiveDryRun();
	}
	return bActive;
#else
	(void)RequestJson;
	(void)bTargetExists;
	return true;
#endif
}

FString FUeremcpBlueprintMutatingGate::Complete(const FUeremcpResponse& Response)
{
#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH
	if (Dispatch.IsSet())
	{
		const FString Serialized = Dispatch->Complete(Response);
		bActive = false;
		Dispatch.Reset();
		return Serialized;
	}
#endif
	return FUeremcpEnvelope::SerializeResponse(Response);
}
