// Editor automation tests for the UeremcpTemplates toolset (WS-15).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "UeremcpTemplatesToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpTemplatesToolsetTests
{
	static bool ParseResponse(
		FAutomationTestBase& Test,
		const FString& Json,
		TSharedPtr<FJsonObject>& OutRoot)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		const bool bParsed =
			FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
		Test.TestTrue(TEXT("response is a JSON object"), bParsed);
		return bParsed;
	}

	static bool ReadStatus(
		FAutomationTestBase& Test,
		const TSharedPtr<FJsonObject>& Root,
		FString& OutStatus)
	{
		const bool bPresent = Root.IsValid()
			&& Root->TryGetStringField(TEXT("status"), OutStatus);
		Test.TestTrue(TEXT("response status is present"), bPresent);
		return bPresent;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTemplatesToolsetRegisterTest,
	"UeremcpTemplates.Toolset.Register",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpTemplatesToolsetRegisterTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpTemplatesToolset::StaticClass()))
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpTemplatesToolset::StaticClass());
	}

	TestTrue(
		TEXT("Templates toolset class is registered"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpTemplatesToolset::StaticClass()));
	TestFalse(
		TEXT("Templates toolset schema is non-empty"),
		UToolsetRegistry::GetToolsetJsonSchema(
			UUeremcpTemplatesToolset::StaticClass()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTemplatesToolsetSearchTest,
	"UeremcpTemplates.Toolset.Search",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpTemplatesToolsetSearchTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"templates-search-1","action":"search_templates","specification":{"query":"projectile","domain":"niagara","limit":10}})");
	const FString ResponseJson = UUeremcpTemplatesToolset::SearchTemplates(Request);

	TSharedPtr<FJsonObject> Root;
	if (!UeremcpTemplatesToolsetTests::ParseResponse(*this, ResponseJson, Root))
	{
		return false;
	}

	FString Status;
	if (!UeremcpTemplatesToolsetTests::ReadStatus(*this, Root, Status))
	{
		return false;
	}
	TestEqual(
		TEXT("search is read-only"),
		Status,
		FString(TEXT("no_change_required")));

	FString Summary;
	TestTrue(TEXT("search summary is present"), Root->TryGetStringField(TEXT("summary"), Summary));
	TestTrue(
		TEXT("search returns the seeded projectile template"),
		Summary.Contains(TEXT("niagara.projectile.elemental.v1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTemplatesToolsetInstantiateValidationTest,
	"UeremcpTemplates.Toolset.Instantiate.Validation",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpTemplatesToolsetInstantiateValidationTest::RunTest(const FString& Parameters)
{
	// Omit the required elemental input so this filter exercises pre-delegation
	// validation without creating cross-domain assets.
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"templates-instantiate-validation-1","action":"instantiate_template","specification":{"template_id":"niagara.projectile.elemental.v1"}})");
	const FString ResponseJson = UUeremcpTemplatesToolset::InstantiateTemplate(Request);

	TSharedPtr<FJsonObject> Root;
	if (!UeremcpTemplatesToolsetTests::ParseResponse(*this, ResponseJson, Root))
	{
		return false;
	}

	FString Status;
	if (!UeremcpTemplatesToolsetTests::ReadStatus(*this, Root, Status))
	{
		return false;
	}
	TestEqual(
		TEXT("missing required input fails validation"),
		Status,
		FString(TEXT("failed_validation")));

	FString Summary;
	TestTrue(
		TEXT("instantiate validation summary is present"),
		Root->TryGetStringField(TEXT("summary"), Summary));
	TestTrue(
		TEXT("summary identifies the missing element input"),
		Summary.Contains(TEXT("element")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTemplatesToolsetPromotePreviewTest,
	"UeremcpTemplates.Toolset.Promote.Preview",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpTemplatesToolsetPromotePreviewTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"templates-promote-preview-1","action":"promote_to_template","specification":{"source_asset":"/Game/__UeremcpTests/Templates/NS_PromotePreview","base_template_id":"niagara.projectile.elemental.v1","quarantine":true},"options":{"dry_run":true}})");
	const FString ResponseJson = UUeremcpTemplatesToolset::PromoteToTemplate(Request);

	TSharedPtr<FJsonObject> Root;
	if (!UeremcpTemplatesToolsetTests::ParseResponse(*this, ResponseJson, Root))
	{
		return false;
	}

	FString Status;
	if (!UeremcpTemplatesToolsetTests::ReadStatus(*this, Root, Status))
	{
		return false;
	}
	TestEqual(
		TEXT("promotion remains an honest preview"),
		Status,
		FString(TEXT("partially_completed")));
	TestFalse(
		TEXT("preview does not report changes"),
		Root->HasField(TEXT("changes")));
	TestFalse(
		TEXT("preview does not claim created_and_validated"),
		ResponseJson.Contains(TEXT("\"status\":\"created_and_validated\"")));
	TestFalse(
		TEXT("preview does not claim modified_and_validated"),
		ResponseJson.Contains(TEXT("\"status\":\"modified_and_validated\"")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
