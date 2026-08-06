// Editor automation tests for niagara.describe_niagara_catalog (WS-07).
//
// The contract these tests defend: every primitive_id the catalog advertises is one
// ResolveModuleAssetPath actually accepts, and every module asset it names really
// loads. A catalog that drifts from the resolver is worse than no catalog.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraModuleResolve.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpNiagaraCatalogTest
{
	static TSharedPtr<FJsonObject> ParseResponse(const FString& Json, FAutomationTestBase& Test)
	{
		TSharedPtr<FJsonObject> Response;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!Test.TestTrue(TEXT("response parses as JSON"), FJsonSerializer::Deserialize(Reader, Response)))
		{
			return nullptr;
		}
		return Response;
	}

	static TSharedPtr<FJsonObject> GetCatalog(
		const TSharedPtr<FJsonObject>& Response,
		FAutomationTestBase& Test)
	{
		if (!Response.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		if (!Test.TestTrue(
				TEXT("response has diagnostics"),
				Response->TryGetObjectField(TEXT("diagnostics"), Diagnostics)))
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Catalog = nullptr;
		if (!Test.TestTrue(
				TEXT("diagnostics has niagara_catalog"),
				(*Diagnostics)->TryGetObjectField(TEXT("niagara_catalog"), Catalog)))
		{
			return nullptr;
		}
		return *Catalog;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCatalogResolverAgreementTest,
	"UEREMCP.Niagara.Catalog.ResolverAgreement",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCatalogResolverAgreementTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraCatalogTest;

	const FString ResponseJson = UUeremcpNiagaraToolset::DescribeNiagaraCatalog(
		TEXT(R"({"protocol_version":"1.0","request_id":"nc-1","action":"describe_niagara_catalog","specification":{"verify_assets":true}})"));

	TSharedPtr<FJsonObject> Response = ParseResponse(ResponseJson, *this);
	if (!Response.IsValid())
	{
		return false;
	}
	TestEqual(
		TEXT("status is no_change_required"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("no_change_required")));

	TSharedPtr<FJsonObject> Catalog = GetCatalog(Response, *this);
	if (!Catalog.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
	if (!TestTrue(TEXT("catalog has modules"), Catalog->TryGetArrayField(TEXT("modules"), Modules) && Modules->Num() > 0))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ModuleValue : *Modules)
	{
		const TSharedPtr<FJsonObject> Module = ModuleValue->AsObject();
		if (!Module.IsValid())
		{
			continue;
		}

		const FString AssetPath = Module->GetStringField(TEXT("asset_path"));
		TestTrue(TEXT("module has asset_path"), !AssetPath.IsEmpty());

		// Every advertised module must actually load. This is the check that catches
		// an engine-version content move before an agent authors against a dead path.
		TestTrue(
			FString::Printf(TEXT("advertised module resolves: %s"), *AssetPath),
			Module->GetBoolField(TEXT("resolves")));

		const TArray<TSharedPtr<FJsonValue>>* PrimitiveIds = nullptr;
		if (!TestTrue(
				FString::Printf(TEXT("module has primitive_ids: %s"), *AssetPath),
				Module->TryGetArrayField(TEXT("primitive_ids"), PrimitiveIds) && PrimitiveIds->Num() > 0))
		{
			continue;
		}

		// Every advertised primitive_id must round-trip through the real resolver
		// back to the same asset the catalog claimed.
		for (const TSharedPtr<FJsonValue>& IdValue : *PrimitiveIds)
		{
			const FString PrimitiveId = IdValue->AsString();
			FString ResolvedPath;
			FString ResolveError;
			const bool bResolved = UeremcpNiagaraModuleResolve::ResolveModuleAssetPath(
				PrimitiveId, FString(), ResolvedPath, ResolveError);

			TestTrue(
				FString::Printf(TEXT("advertised primitive_id resolves: %s"), *PrimitiveId),
				bResolved);
			TestEqual(
				FString::Printf(TEXT("primitive_id maps to advertised asset: %s"), *PrimitiveId),
				ResolvedPath,
				AssetPath);
		}

		TestTrue(
			FString::Printf(TEXT("module has default_script_usage: %s"), *AssetPath),
			!Module->GetStringField(TEXT("default_script_usage")).IsEmpty());
	}

	// Vocabulary lists must be present and non-empty — they are the whole point.
	for (const TCHAR* Field : { TEXT("renderer_hints"), TEXT("script_usages"), TEXT("input_modes") })
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		TestTrue(
			FString::Printf(TEXT("catalog has non-empty %s"), Field),
			Catalog->TryGetArrayField(Field, Values) && Values->Num() > 0);
	}

	TestTrue(
		TEXT("catalog names the emitter substrate"),
		!Catalog->GetStringField(TEXT("emitter_substrate")).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCatalogSearchTest,
	"UEREMCP.Niagara.Catalog.SearchAndOptions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCatalogSearchTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraCatalogTest;

	// search narrows, and every survivor matches.
	{
		TSharedPtr<FJsonObject> Catalog = GetCatalog(
			ParseResponse(
				UUeremcpNiagaraToolset::DescribeNiagaraCatalog(
					TEXT(R"({"protocol_version":"1.0","action":"describe_niagara_catalog","specification":{"search":"spawn","verify_assets":false}})")),
				*this),
			*this);
		if (!Catalog.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!TestTrue(
				TEXT("search 'spawn' matched at least one module"),
				Catalog->TryGetArrayField(TEXT("modules"), Modules) && Modules->Num() > 0))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& ModuleValue : *Modules)
		{
			const TSharedPtr<FJsonObject> Module = ModuleValue->AsObject();
			if (!Module.IsValid())
			{
				continue;
			}
			bool bMatches = Module->GetStringField(TEXT("asset_path")).Contains(TEXT("spawn"), ESearchCase::IgnoreCase);
			const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
			if (Module->TryGetArrayField(TEXT("primitive_ids"), Ids))
			{
				for (const TSharedPtr<FJsonValue>& Id : *Ids)
				{
					bMatches = bMatches || Id->AsString().Contains(TEXT("spawn"), ESearchCase::IgnoreCase);
				}
			}
			TestTrue(TEXT("every search survivor matches the term"), bMatches);

			// verify_assets=false must not claim verification it did not do.
			TestFalse(
				TEXT("verify_assets=false omits the resolves claim"),
				Module->HasField(TEXT("resolves")));
		}

		TestFalse(TEXT("assets_verified reported false"), Catalog->GetBoolField(TEXT("assets_verified")));
	}

	// An unmatched search returns an empty catalog, not an error.
	{
		TSharedPtr<FJsonObject> Catalog = GetCatalog(
			ParseResponse(
				UUeremcpNiagaraToolset::DescribeNiagaraCatalog(
					TEXT(R"({"protocol_version":"1.0","action":"describe_niagara_catalog","specification":{"search":"zzzz_no_such_module"}})")),
				*this),
			*this);
		if (!Catalog.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		Catalog->TryGetArrayField(TEXT("modules"), Modules);
		TestTrue(TEXT("unmatched search returns zero modules"), Modules == nullptr || Modules->Num() == 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCatalogRejectionTest,
	"UEREMCP.Niagara.Catalog.Rejections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCatalogRejectionTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraCatalogTest;

	{
		TSharedPtr<FJsonObject> Response = ParseResponse(
			UUeremcpNiagaraToolset::DescribeNiagaraCatalog(
				TEXT(R"({"protocol_version":"1.0","action":"inspect_system","specification":{}})")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("wrong action is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	{
		TSharedPtr<FJsonObject> Response = ParseResponse(
			UUeremcpNiagaraToolset::DescribeNiagaraCatalog(TEXT("{not json")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("malformed envelope is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
