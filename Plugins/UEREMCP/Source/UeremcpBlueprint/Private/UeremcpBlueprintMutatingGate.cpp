#include "UeremcpBlueprintMutatingGate.h"

#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH
#include "UeremcpMutatingDispatch.h"

class FUeremcpBlueprintMutatingGate::FDispatchHolder
{
public:
	FUeremcpMutatingDispatch Instance;
};
#endif

bool FUeremcpBlueprintMutatingGate::TryBeginRead(
	const FString& RequestJson,
	FString& OutBlockingResponseJson)
{
	OutBlockingResponseJson.Reset();
	bActive = false;
	bEffectiveDryRun = false;

#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH
	Dispatch = MakeUnique<FDispatchHolder>();
	bActive = Dispatch->Instance.TryBegin(
		RequestJson,
		true,
		0,
		true,
		OutBlockingResponseJson);
	if (bActive)
	{
		bEffectiveDryRun = Dispatch->Instance.IsEffectiveDryRun();
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
	Dispatch = MakeUnique<FDispatchHolder>();
	bActive = Dispatch->Instance.TryBegin(
		RequestJson,
		bTargetExists,
		0,
		false,
		OutBlockingResponseJson);
	if (bActive)
	{
		bEffectiveDryRun = Dispatch->Instance.IsEffectiveDryRun();
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
	if (Dispatch.IsValid())
	{
		const FString Serialized = Dispatch->Instance.Complete(Response);
		bActive = false;
		Dispatch.Reset();
		return Serialized;
	}
#endif
	return FUeremcpEnvelope::SerializeResponse(Response);
}
