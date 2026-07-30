// Editor automation tests for the UeremcpTemplates toolset (WS-15).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/SoftObjectPath.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTemplatesPocCThirdGenerationTest,
	"UEREMCP.Templates.POCC.ThirdGeneration",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpTemplatesPocCThirdGenerationTest::RunTest(const FString& Parameters)
{
	struct FGeneration
	{
		FString RequestId;
		FString Element;
		FString SourcePath;
		FString TargetPath;
		FString ModifiersJson;
	};
	const TArray<FGeneration> Generations = {
		{
			TEXT("poc-c-ice-live"),
			TEXT("ice"),
			TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball"),
			TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariation"),
			TEXT("\"adjust\":[\"reduce_trail_persistence\",\"boost_impact\"],\"add\":[\"crystalline_fragments\"],\"preserve\":[\"preserve_networking\"]")
		},
		{
			TEXT("poc-c-wind-live"),
			TEXT("wind"),
			TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariation"),
			TEXT("/Game/__UeremcpPoc/NS_POCC_WindThirdGeneration"),
			TEXT("\"preserve\":[\"preserve_networking\"]")
		},
	};

	for (const FGeneration& Generation : Generations)
	{
		const FString Request = FString::Printf(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"%s\",\"action\":\"instantiate_template\",")
			TEXT("\"mode\":\"replace\",\"specification\":{\"template_id\":\"niagara.projectile.elemental.v1\",")
			TEXT("\"inputs\":{\"element\":\"%s\",\"target_path\":\"%s\",\"source_system\":\"%s\",\"scale\":1.0,\"intensity\":6.0},")
			TEXT("\"modifiers\":{%s},\"target\":{\"asset_path\":\"%s\"},\"mode\":\"replace\"},")
			TEXT("\"options\":{\"dry_run\":false,\"allow_destructive\":true,\"atomic\":true,\"rollback_on_failure\":true,")
			TEXT("\"compile\":true,\"validate\":true,\"save\":true,\"response_detail\":\"complete\",\"timeout_ms\":0}}"),
			*Generation.RequestId,
			*Generation.Element,
			*Generation.TargetPath,
			*Generation.SourcePath,
			*Generation.ModifiersJson,
			*Generation.TargetPath);

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
		TestTrue(
			*FString::Printf(TEXT("%s reached an honest terminal response"), *Generation.Element),
			Status == TEXT("created_and_validated")
				|| Status == TEXT("modified_and_validated")
				|| Status == TEXT("partially_completed"));

		const TSharedPtr<FJsonObject>* Metrics = nullptr;
		double RoundTrips = 0.0;
		TestTrue(
			TEXT("generation reports one MCP round trip"),
			Root->TryGetObjectField(TEXT("metrics"), Metrics)
				&& Metrics
				&& (*Metrics)->TryGetNumberField(TEXT("mcp_round_trips"), RoundTrips)
				&& RoundTrips == 1.0);

		const TSharedPtr<FJsonObject>* Understood = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
		bool bInherited = false;
		bool bOverridden = false;
		if (Root->TryGetObjectField(TEXT("understood"), Understood)
			&& Understood
			&& (*Understood)->TryGetArrayField(TEXT("interpretation_notes"), Notes)
			&& Notes)
		{
			for (const TSharedPtr<FJsonValue>& Note : *Notes)
			{
				FString Text;
				if (Note.IsValid() && Note->TryGetString(Text))
				{
					bInherited |= Text.StartsWith(TEXT("inherited:"));
					bOverridden |= Text.StartsWith(TEXT("overridden:"));
				}
			}
		}
		TestTrue(TEXT("response reports inherited pattern facts"), bInherited);
		TestTrue(TEXT("response reports overridden variation facts"), bOverridden);
		TestNotNull(
			*FString::Printf(TEXT("%s generation asset exists"), *Generation.Element),
			FSoftObjectPath(Generation.TargetPath).TryLoad());
	}
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
