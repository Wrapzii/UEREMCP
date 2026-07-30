// execute_plan transaction coordinator tests (WS-03 / ADR-0005).

#include "CoreMinimal.h"

#include "Misc/AutomationTest.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "UeremcpPlanExecutor.h"
#include "UeremcpPlanTransactionCoordinator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanTransactionCoordinatorRegistrationTest,
	"UeremcpCore.PlanTransaction.Registered",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanTransactionCoordinatorRegistrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FString Error;
	TestTrue(
		TEXT("register transaction callbacks"),
		FUeremcpPlanTransactionCoordinator::RegisterWithExecutor(Error));
	TestTrue(TEXT("registration error empty"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanTransactionCoordinatorSandboxLifecycleTest,
	"UeremcpCore.PlanTransaction.SandboxLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanTransactionCoordinatorSandboxLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	using namespace UE::ToolsetRegistry;

	if (FGlobalSandbox::IsActive())
	{
		FGlobalSandbox::Discard();
		FGlobalSandbox::Leave();
	}

	FString Error;
	TestTrue(TEXT("begin transaction"), FUeremcpPlanTransactionCoordinator::Begin(Error));
	TestTrue(TEXT("sandbox active after begin"), FGlobalSandbox::IsActive());
	TestTrue(TEXT("session active after begin"), FUeremcpPlanTransactionCoordinator::IsSessionActive());

	TestTrue(TEXT("commit transaction"), FUeremcpPlanTransactionCoordinator::Commit(Error));
	TestFalse(TEXT("sandbox inactive after commit"), FGlobalSandbox::IsActive());
	TestFalse(TEXT("session inactive after commit"), FUeremcpPlanTransactionCoordinator::IsSessionActive());

	TestTrue(TEXT("begin second transaction"), FUeremcpPlanTransactionCoordinator::Begin(Error));
	TestTrue(TEXT("rollback transaction"), FUeremcpPlanTransactionCoordinator::Rollback(Error));
	TestFalse(TEXT("sandbox inactive after rollback"), FGlobalSandbox::IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPlanTransactionCoordinatorRejectNestedBeginTest,
	"UeremcpCore.PlanTransaction.RejectNestedBegin",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPlanTransactionCoordinatorRejectNestedBeginTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	using namespace UE::ToolsetRegistry;

	if (FGlobalSandbox::IsActive())
	{
		FGlobalSandbox::Discard();
		FGlobalSandbox::Leave();
	}

	FString Error;
	TestTrue(TEXT("enter foreign sandbox"), FGlobalSandbox::Enter(TEXT("foreign_sandbox"), TEXT("test")));
	TestFalse(TEXT("begin blocked by active sandbox"), FUeremcpPlanTransactionCoordinator::Begin(Error));
	TestFalse(TEXT("error empty when blocked"), Error.IsEmpty());
	FGlobalSandbox::Discard();
	FGlobalSandbox::Leave();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
