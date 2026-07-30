// Editor automation tests for UeremcpNiagara material binding (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraMaterialBinding.h"
#include "UeremcpNiagaraRoleNames.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraMaterialBindingOfflineTest,
	"UEREMCP.Niagara.Create.MaterialBindingOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraMaterialBindingOfflineTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("role mapping"), UeremcpNiagaraRoles::RoleToEmitterName(TEXT("ribbon_trail")), FString(TEXT("RibbonTrail")));

	TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();
	Materials->SetStringField(TEXT("core"), TEXT("/Game/__UeremcpTests/Materials/MI_Core"));
	TSharedPtr<FJsonObject> Trail = MakeShared<FJsonObject>();
	Trail->SetObjectField(TEXT("create_spec"), MakeShared<FJsonObject>());
	Materials->SetObjectField(TEXT("ribbon_trail"), Trail);
	Spec->SetObjectField(TEXT("materials"), Materials);

	TArray<FUeremcpNiagaraMaterialRequest> Requests;
	FString Error;
	TestTrue(TEXT("parse materials"), FUeremcpNiagaraMaterialBinding::ParseMaterialRequests(Spec, Requests, Error));
	TestEqual(TEXT("two requests"), Requests.Num(), 2);

	TSharedPtr<FJsonObject> SpriteValues = MakeShared<FJsonObject>();
	SpriteValues->SetStringField(TEXT("Alignment"), TEXT("Automatic"));
	FString Conflict;
	TestTrue(
		TEXT("patch sprite material"),
		FUeremcpNiagaraMaterialBinding::PatchSpriteOrRibbonMaterial(
			SpriteValues,
			TEXT("/Game/__UeremcpTests/Materials/MI_Core.MI_Core"),
			Conflict));

	const TSharedPtr<FJsonObject>* MaterialRef = nullptr;
	TestTrue(TEXT("Material refPath set"), SpriteValues->TryGetObjectField(TEXT("Material"), MaterialRef));
	FString RefPath;
	TestTrue(TEXT("refPath present"), (*MaterialRef)->TryGetStringField(TEXT("refPath"), RefPath));
	TestEqual(TEXT("canonical path"), RefPath, FString(TEXT("/Game/__UeremcpTests/Materials/MI_Core.MI_Core")));

	TSharedPtr<FJsonObject> Bound = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Binding = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Parameter = MakeShared<FJsonObject>();
	Parameter->SetStringField(TEXT("Name"), TEXT("User.Material"));
	Binding->SetObjectField(TEXT("Parameter"), Parameter);
	Bound->SetObjectField(TEXT("MaterialUserParamBinding"), Binding);
	TestTrue(
		TEXT("user binding conflict"),
		FUeremcpNiagaraMaterialBinding::HasValidUserMaterialBinding(Bound, TEXT("MaterialUserParamBinding")));
	TestFalse(
		TEXT("patch blocked by user binding"),
		FUeremcpNiagaraMaterialBinding::PatchSpriteOrRibbonMaterial(
			Bound,
			TEXT("/Game/__UeremcpTests/Materials/MI_Core.MI_Core"),
			Conflict));

	TMap<FString, FString> Resolved;
	TArray<FString> Unresolved;
	FUeremcpNiagaraMaterialRequest BadRequest;
	BadRequest.Role = TEXT("core");
	BadRequest.ExistingAssetPath = TEXT("/Game/VFX/M_Bad");
	const TArray<FUeremcpNiagaraMaterialRequest> BadRequests = {BadRequest};
	TestFalse(
		TEXT("reject material outside probe root"),
		FUeremcpNiagaraMaterialBinding::ResolveDirectMaterialPaths(
			BadRequests,
			Resolved,
			Unresolved,
			Error));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
