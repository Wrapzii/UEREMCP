// Editor automation tests for UeremcpNiagara material binding (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraMaterialBinding.h"
#include "UeremcpNiagaraMaterialBindingDiagnostics.h"
#include "UeremcpNiagaraRoleNames.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpNiagaraMaterialBindingTest
{
	TSharedPtr<FJsonObject> MakeMixedMaterialsSpec()
	{
		TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();
		Materials->SetStringField(TEXT("sparks"), TEXT("/Game/__UeremcpTests/Materials/MI_Sparks"));

		TSharedPtr<FJsonObject> Trail = MakeShared<FJsonObject>();
		Trail->SetBoolField(TEXT("reuse_if_present"), true);
		TSharedPtr<FJsonObject> CreateSpec = MakeShared<FJsonObject>();
		CreateSpec->SetStringField(TEXT("purpose"), TEXT("elemental_projectile_trail"));
		CreateSpec->SetStringField(TEXT("element"), TEXT("fire"));
		Trail->SetObjectField(TEXT("create_spec"), CreateSpec);
		Materials->SetObjectField(TEXT("ribbon_trail"), Trail);

		Spec->SetObjectField(TEXT("materials"), Materials);
		return Spec;
	}

	bool InlineEntryHasField(
		const TSharedPtr<FJsonObject>& Entry,
		const TCHAR* FieldName,
		EJson ExpectedType)
	{
		if (!Entry.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonValue> Field = Entry->TryGetField(FieldName);
		return Field.IsValid() && Field->Type == ExpectedType;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraMaterialBindingOfflineTest,
	"UEREMCP.Niagara.Create.MaterialBindingOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraMaterialBindingOfflineTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("role mapping"), UeremcpNiagaraRoles::RoleToEmitterName(TEXT("ribbon_trail")), FString(TEXT("RibbonTrail")));

	TSharedPtr<FJsonObject> Spec = UeremcpNiagaraMaterialBindingTest::MakeMixedMaterialsSpec();
	TArray<FUeremcpNiagaraMaterialRequest> Requests;
	FString Error;
	TestTrue(TEXT("parse materials"), FUeremcpNiagaraMaterialBinding::ParseMaterialRequests(Spec, Requests, Error));
	TestEqual(TEXT("two requests"), Requests.Num(), 2);

	const FUeremcpNiagaraMaterialRequest* SparksRequest = Requests.FindByPredicate(
		[](const FUeremcpNiagaraMaterialRequest& Request) { return Request.Role == TEXT("sparks"); });
	const FUeremcpNiagaraMaterialRequest* TrailRequest = Requests.FindByPredicate(
		[](const FUeremcpNiagaraMaterialRequest& Request) { return Request.Role == TEXT("ribbon_trail"); });
	TestNotNull(TEXT("sparks request"), SparksRequest);
	TestNotNull(TEXT("ribbon_trail request"), TrailRequest);
	if (SparksRequest)
	{
		TestEqual(
			TEXT("sparks direct path"),
			SparksRequest->ExistingAssetPath,
			FString(TEXT("/Game/__UeremcpTests/Materials/MI_Sparks")));
		TestFalse(TEXT("sparks has no create_spec"), SparksRequest->CreateSpec.IsValid());
	}
	if (TrailRequest)
	{
		TestTrue(TEXT("ribbon_trail create_spec"), TrailRequest->CreateSpec.IsValid());
		TestTrue(TEXT("ribbon_trail reuse_if_present"), TrailRequest->bReuseIfPresent);
		FString Purpose;
		TestTrue(
			TEXT("ribbon_trail purpose preserved"),
			TrailRequest->CreateSpec->TryGetStringField(TEXT("purpose"), Purpose));
		TestEqual(TEXT("purpose value"), Purpose, FString(TEXT("elemental_projectile_trail")));
	}

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

	TMap<FString, FString> DirectOnlyResolved;
	TArray<FString> DirectOnlyUnresolved;
	TestTrue(
		TEXT("create_spec skipped by direct resolver"),
		FUeremcpNiagaraMaterialBinding::ResolveDirectMaterialPaths(
			Requests,
			DirectOnlyResolved,
			DirectOnlyUnresolved,
			Error));
	TestEqual(TEXT("direct resolver skips create_spec"), DirectOnlyResolved.Num(), 0);
	TestEqual(TEXT("direct resolver no unresolved create_spec"), DirectOnlyUnresolved.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraMaterialBindingDiagnosticsOfflineTest,
	"UEREMCP.Niagara.Create.MaterialBindingDiagnosticsOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraMaterialBindingDiagnosticsOfflineTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraMaterialBindingResult Result;
	Result.ResolvedMaterialPaths.Add(
		TEXT("sparks"),
		TEXT("/Game/__UeremcpTests/Materials/MI_Sparks.MI_Sparks"));
	Result.RendererBindingsApplied.Add(TEXT("sparks/renderer_0"));
	Result.RendererBindingsVerified.Add(TEXT("sparks/renderer_0"));

	FUeremcpNiagaraInlineMaterialCreate InlineCreate;
	InlineCreate.Role = TEXT("ribbon_trail");
	InlineCreate.bSuccess = true;
	InlineCreate.Status = TEXT("partially_completed");
	InlineCreate.Summary = TEXT("Created probe MI; Niagara bind not verified.");
	InlineCreate.PrimaryAsset = TEXT("/Game/__UeremcpTests/Materials/MI_Probe_RibbonTrail");
	InlineCreate.CapabilityNotes.Add(TEXT("create_vfx_material probe slice only."));
	FUeremcpAssetRef CreatedAsset;
	CreatedAsset.AssetPath = InlineCreate.PrimaryAsset;
	CreatedAsset.AssetClass = TEXT("MaterialInstanceConstant");
	InlineCreate.CreatedAssets.Add(CreatedAsset);
	Result.InlineMaterialCreates.Add(InlineCreate);
	Result.UnresolvedMaterialBindings.Add(
		TEXT("ribbon_trail: renderer 0 re-read material path mismatch"));

	const TArray<FString> Orphans =
		FUeremcpNiagaraMaterialBindingDiagnostics::FindOrphanedInlineCreates(Result);
	TestEqual(TEXT("one orphaned inline create"), Orphans.Num(), 1);
	TestEqual(TEXT("orphaned role"), Orphans[0], FString(TEXT("ribbon_trail")));

	const TSharedPtr<FJsonObject> Materials =
		FUeremcpNiagaraMaterialBindingDiagnostics::BuildMaterialBindingsObject(Result);
	TestTrue(TEXT("materials diagnostics object"), Materials.IsValid());

	const TSharedPtr<FJsonObject>* ResolvedPaths = nullptr;
	TestTrue(TEXT("resolved_paths present"), Materials->TryGetObjectField(TEXT("resolved_paths"), ResolvedPaths));
	FString SparksPath;
	TestTrue(
		TEXT("sparks resolved"),
		ResolvedPaths && (*ResolvedPaths)->TryGetStringField(TEXT("sparks"), SparksPath));
	TestEqual(TEXT("sparks canonical path"), SparksPath, FString(TEXT("/Game/__UeremcpTests/Materials/MI_Sparks.MI_Sparks")));

	const TArray<TSharedPtr<FJsonValue>>* InlineCreates = nullptr;
	TestTrue(
		TEXT("inline_material_creates array"),
		Materials->TryGetArrayField(TEXT("inline_material_creates"), InlineCreates));
	TestTrue(TEXT("one inline create entry"), InlineCreates && InlineCreates->Num() == 1);

	const TSharedPtr<FJsonObject> InlineEntry = (*InlineCreates)[0]->AsObject();
	TestTrue(
		TEXT("inline role field"),
		UeremcpNiagaraMaterialBindingTest::InlineEntryHasField(InlineEntry, TEXT("role"), EJson::String));
	TestTrue(
		TEXT("inline success field"),
		UeremcpNiagaraMaterialBindingTest::InlineEntryHasField(InlineEntry, TEXT("success"), EJson::Boolean));
	TestTrue(
		TEXT("inline status field"),
		UeremcpNiagaraMaterialBindingTest::InlineEntryHasField(InlineEntry, TEXT("status"), EJson::String));
	TestTrue(
		TEXT("inline primary_asset field"),
		UeremcpNiagaraMaterialBindingTest::InlineEntryHasField(InlineEntry, TEXT("primary_asset"), EJson::String));

	FString InlineStatus;
	TestTrue(TEXT("inline status value"), InlineEntry->TryGetStringField(TEXT("status"), InlineStatus));
	TestEqual(TEXT("honest inline status"), InlineStatus, FString(TEXT("partially_completed")));

	const TArray<TSharedPtr<FJsonValue>>* CreatedAssets = nullptr;
	TestTrue(
		TEXT("inline created_assets array"),
		InlineEntry->TryGetArrayField(TEXT("created_assets"), CreatedAssets));
	TestTrue(TEXT("one created asset"), CreatedAssets && CreatedAssets->Num() == 1);
	const TSharedPtr<FJsonObject> CreatedAssetObj = (*CreatedAssets)[0]->AsObject();
	FString CreatedAssetPath;
	TestTrue(
		TEXT("created asset_path"),
		CreatedAssetObj->TryGetStringField(TEXT("asset_path"), CreatedAssetPath));
	TestEqual(
		TEXT("created asset path value"),
		CreatedAssetPath,
		FString(TEXT("/Game/__UeremcpTests/Materials/MI_Probe_RibbonTrail")));

	const TArray<TSharedPtr<FJsonValue>>* OrphanedField = nullptr;
	TestTrue(
		TEXT("orphaned_inline_creates array"),
		Materials->TryGetArrayField(TEXT("orphaned_inline_creates"), OrphanedField));
	TestTrue(TEXT("orphaned role listed"), OrphanedField && OrphanedField->Num() == 1);
	TestEqual(
		TEXT("orphaned role value"),
		(*OrphanedField)[0]->AsString(),
		FString(TEXT("ribbon_trail")));

	FUeremcpNiagaraMaterialBindingResult EmptyResult;
	TestNull(
		TEXT("empty result yields null diagnostics"),
		FUeremcpNiagaraMaterialBindingDiagnostics::BuildMaterialBindingsObject(EmptyResult).Get());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
