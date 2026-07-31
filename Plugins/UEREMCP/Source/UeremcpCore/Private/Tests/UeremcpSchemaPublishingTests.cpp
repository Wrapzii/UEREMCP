// UEREMCP — nested schema publishing tests (BACKLOG 1.2a / 1b.1).

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "UeremcpSchemaPublishing.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSchemaPublishingNestedEnvelopeTest,
	"UeremcpCore.SchemaPublishing.NestedEnvelopeForTool",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSchemaPublishingNestedEnvelopeTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Schema =
		UeremcpSchemaPublishing::BuildNestedRequestSchemaForTool(TEXT("BuildEnvironment"));
	TestTrue(TEXT("schema object built"), Schema.IsValid());
	if (!Schema.IsValid())
	{
		return false;
	}

	FString Type;
	TestTrue(TEXT("type=object"), Schema->TryGetStringField(TEXT("type"), Type) && Type.Equals(TEXT("object")));

	const TSharedPtr<FJsonObject>* Props = nullptr;
	TestTrue(TEXT("has properties"), Schema->TryGetObjectField(TEXT("properties"), Props) && Props);
	if (!Props)
	{
		return false;
	}

	TestTrue(TEXT("has protocol_version"), (*Props)->HasField(TEXT("protocol_version")));
	TestTrue(TEXT("has action"), (*Props)->HasField(TEXT("action")));
	TestTrue(TEXT("has specification"), (*Props)->HasField(TEXT("specification")));
	TestTrue(TEXT("has options"), (*Props)->HasField(TEXT("options")));

	const TSharedPtr<FJsonObject>* Action = nullptr;
	TestTrue(TEXT("action object"), (*Props)->TryGetObjectField(TEXT("action"), Action) && Action);
	if (Action)
	{
		FString ActionConst;
		TestTrue(
			TEXT("action const=build_environment"),
			(*Action)->TryGetStringField(TEXT("const"), ActionConst)
				&& ActionConst.Equals(TEXT("build_environment")));
	}

	const TSharedPtr<FJsonObject>* Spec = nullptr;
	TestTrue(TEXT("specification object"), (*Props)->TryGetObjectField(TEXT("specification"), Spec) && Spec);
	if (Spec)
	{
		TestTrue(TEXT("specification declares seed"), (*Spec)->HasField(TEXT("properties")));
		const TSharedPtr<FJsonObject>* SpecProps = nullptr;
		if ((*Spec)->TryGetObjectField(TEXT("properties"), SpecProps) && SpecProps)
		{
			TestTrue(TEXT("seed required field present in domain schema"), (*SpecProps)->HasField(TEXT("seed")));
		}
	}

	const FString Normalized = UeremcpSchemaPublishing::NormalizeArgumentsToRequestJson(
		TEXT("{\"protocol_version\":\"1.0\",\"action\":\"echo\",\"specification\":{}}"));
	TestTrue(TEXT("normalize wraps nested envelope"), Normalized.Contains(TEXT("requestJson")));
	TestTrue(TEXT("normalize keeps envelope body"), Normalized.Contains(TEXT("protocol_version")));

	const FString Legacy = UeremcpSchemaPublishing::NormalizeArgumentsToRequestJson(
		TEXT("{\"requestJson\":\"{\\\"protocol_version\\\":\\\"1.0\\\",\\\"action\\\":\\\"echo\\\"}\"}"));
	TestTrue(TEXT("legacy requestJson passthrough"), Legacy.Contains(TEXT("requestJson")));

	return true;
}
