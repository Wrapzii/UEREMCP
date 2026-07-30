// Editor automation tests for UeremcpNiagara inspect mapping (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "UeremcpNiagaraInspectMapping.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraEventHandlerPlaceholderTest,
	"UEREMCP.Niagara.Inspect.EventHandlerPlaceholders",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraEventHandlerPlaceholderTest::RunTest(const FString& Parameters)
{
	FNiagaraExt_StackIssues Issues;
	FNiagaraExt_StackIssue Issue;
	Issue.StackDisplayPath = TEXT("ProbeBurst/Event Handler - Source: DeathEvent");
	Issue.Location.EmitterName = FName(TEXT("ProbeBurst"));
	Issue.Location.ScriptName = FName(TEXT("ParticleEventScript"));
	Issues.Issues.Add(Issue);

	FNiagaraExt_SystemCompileState Compile;
	FNiagaraExt_ScriptCompileInfo ScriptInfo;
	ScriptInfo.EmitterName = FName(TEXT("ProbeBurst"));
	ScriptInfo.ScriptName = FName(TEXT("ParticleEventScript"));
	Compile.Scripts.Add(ScriptInfo);

	const TArray<TSharedPtr<FJsonValue>> Handlers =
		FUeremcpNiagaraInspectMapping::BuildEventHandlerPlaceholders(Issues, Compile);

	TestTrue(TEXT("at least one event handler placeholder"), Handlers.Num() >= 1);
	if (Handlers.Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Handler = Handlers[0]->AsObject();
	TestTrue(TEXT("handler object"), Handler.IsValid());
	if (!Handler.IsValid())
	{
		return false;
	}

	FString ScriptUsage;
	TestTrue(TEXT("script_usage ParticleEventScript"), Handler->TryGetStringField(TEXT("script_usage"), ScriptUsage));
	TestEqual(TEXT("script_usage value"), ScriptUsage, FString(TEXT("ParticleEventScript")));

	const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
	TestTrue(TEXT("modules array present"), Handler->TryGetArrayField(TEXT("modules"), Modules));
	TestTrue(TEXT("modules empty (lossy)"), Modules && Modules->Num() == 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
