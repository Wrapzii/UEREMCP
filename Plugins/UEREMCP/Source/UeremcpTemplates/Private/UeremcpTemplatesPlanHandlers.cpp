// UEREMCP — execute_plan semantic handlers for the Templates domain.
//
// Being AICallable and being usable inside execute_plan are two separate
// registries. A tool present in one and absent from the other fails only at
// plan time -- "no handler registered for '<action>'" -- after the agent has
// already committed to a batch and has to abandon it.
//
// Found by tools/check_operation_catalog.py, which asserts every routable
// destructive catalog action has a binding here.
//
// promote_to_template is what lets an agent BANK a result it just proved.
// Without a plan binding it could never be the last step of the plan that
// produced the thing worth banking.

#include "UeremcpTemplatesPlanHandlers.h"

#include "UeremcpTemplatesToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	using FTemplatesToolFn = FString (*)(const FString&);

	bool DispatchTool(
		const TCHAR* Action,
		FTemplatesToolFn Fn,
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

bool FUeremcpTemplatesPlanHandlers::Register(FString& OutError)
{
	auto Bind = [&OutError](const TCHAR* Action, FTemplatesToolFn Fn) -> bool
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

	if (!Bind(TEXT("search_templates"), &UUeremcpTemplatesToolset::SearchTemplates)) return false;
	if (!Bind(TEXT("instantiate_template"), &UUeremcpTemplatesToolset::InstantiateTemplate)) return false;
	if (!Bind(TEXT("promote_to_template"), &UUeremcpTemplatesToolset::PromoteToTemplate)) return false;
	if (!Bind(TEXT("create_template"), &UUeremcpTemplatesToolset::CreateTemplate)) return false;
	if (!Bind(TEXT("update_template"), &UUeremcpTemplatesToolset::UpdateTemplate)) return false;
	OutError.Reset();
	return true;
}

void FUeremcpTemplatesPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(TEXT("search_templates"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("instantiate_template"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("promote_to_template"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("create_template"));
	FUeremcpPlanExecutor::UnregisterAction(TEXT("update_template"));
}
