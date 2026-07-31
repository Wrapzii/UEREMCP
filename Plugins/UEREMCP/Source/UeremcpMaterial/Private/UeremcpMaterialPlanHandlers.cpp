// UEREMCP — execute_plan semantic handlers for the Material domain (WS-08).
//
// Registering a tool as AICallable does NOT make it usable inside execute_plan.
// Those are two separate registries, and a tool present in one and absent from
// the other fails only at plan time, with "no handler for <action>" — after the
// agent has already committed to a batch.
//
// Measured: an agent authoring meshes from scratch built a correct
// texture -> material -> mesh -> scatter plan, then had to abandon it and issue
// the calls one at a time, because only create_vfx_material was registered here.
// The whole point of the cascade is that it runs as one transaction.

#include "UeremcpMaterialPlanHandlers.h"

#include "UeremcpMaterialToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FMaterialToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const TCHAR* Action,
		FMaterialToolFn Fn,
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = Fn(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s returned an empty response"), Action);
			return false;
		}
		return true;
	}
}

const TCHAR* FUeremcpMaterialPlanHandlers::RegisteredActionName()
{
	return TEXT("create_vfx_material");
}

bool FUeremcpMaterialPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&OutError](const TCHAR* Action, FMaterialToolFn Fn) -> bool
	{
		FString LocalError;
		const bool bOk = FUeremcpPlanExecutor::RegisterAction(
			Action,
			[Action, Fn](const FString& RequestJson, FString& OutResponseJson, FString& Err) -> bool
			{
				return DispatchTool(Action, Fn, RequestJson, OutResponseJson, Err);
			},
			LocalError);
		if (!bOk)
		{
			OutError = LocalError;
		}
		return bOk;
	};

	// create_procedural_texture is the primitive floor: from an empty project it
	// is the first operation in every material cascade. It was AICallable but
	// never plan-registered, so no cascade could run as a batch.
	if (!Bind(TEXT("create_procedural_texture"), &UUeremcpMaterialToolset::CreateProceduralTexture)) return false;
	if (!Bind(TEXT("create_master_material"), &UUeremcpMaterialToolset::CreateMasterMaterial)) return false;
	if (!Bind(TEXT("create_vfx_material"), &UUeremcpMaterialToolset::CreateVfxMaterial)) return false;

	OutError.Reset();
	return true;
}

void FUeremcpMaterialPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(TEXT("create_procedural_texture"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("create_master_material"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("create_vfx_material"));
}
