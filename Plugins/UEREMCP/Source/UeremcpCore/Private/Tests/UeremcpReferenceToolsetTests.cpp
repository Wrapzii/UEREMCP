// Editor automation tests for UeremcpCore reference toolset (ADR-0002 / RB-03).

// Lives under Source/UeremcpCore (WS-03 owned). WS-11 owns tests/** — do not put these there.



#include "CoreMinimal.h"

#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"

#include "Misc/Paths.h"

#include "Dom/JsonObject.h"

#include "Serialization/JsonReader.h"

#include "Serialization/JsonSerializer.h"



#include "ToolsetRegistry/UToolsetRegistry.h"

#include "UeremcpReferenceToolset.h"

#include "UeremcpMinimalEnvelope.h"



#if WITH_DEV_AUTOMATION_TESTS



IMPLEMENT_SIMPLE_AUTOMATION_TEST(

	FUeremcpReferenceToolsetPingTest,

	"UeremcpCore.ReferenceToolset.Ping",

	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)



bool FUeremcpReferenceToolsetPingTest::RunTest(const FString& Parameters)

{

	const FString Json = UUeremcpReferenceToolset::Ping();

	TSharedPtr<FJsonObject> Root;

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	TestTrue(TEXT("Ping returns parseable JSON object"),

		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());

	if (!Root.IsValid())

	{

		return false;

	}



	FString Status;

	TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Status));

	TestEqual(TEXT("status is no_change_required"), Status, FString(TEXT("no_change_required")));



	const TSharedPtr<FJsonObject>* Metrics = nullptr;

	TestTrue(TEXT("metrics present"), Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid());

	if (Metrics && Metrics->IsValid())

	{

		TestEqual(TEXT("mcp_round_trips == 1"),

			static_cast<int32>((*Metrics)->GetNumberField(TEXT("mcp_round_trips"))), 1);

	}

	return true;

}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(

	FUeremcpReferenceToolsetEchoTest,

	"UeremcpCore.ReferenceToolset.Echo",

	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)



bool FUeremcpReferenceToolsetEchoTest::RunTest(const FString& Parameters)

{

	const FString Request = TEXT(

		R"({"protocol_version":"1.0.0","request_id":"echo-1","action":"reference.echo","target":{"asset_path":"/Game/None"}})");

	const FString Json = UUeremcpReferenceToolset::Echo(Request);



	TSharedPtr<FJsonObject> Root;

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	TestTrue(TEXT("Echo returns parseable JSON"),

		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());

	if (!Root.IsValid())

	{

		return false;

	}



	FString Status;

	FString RequestId;

	Root->TryGetStringField(TEXT("status"), Status);

	Root->TryGetStringField(TEXT("request_id"), RequestId);

	TestEqual(TEXT("status"), Status, FString(TEXT("no_change_required")));

	TestEqual(TEXT("request_id echoed"), RequestId, FString(TEXT("echo-1")));



	const FString Rejected = UUeremcpReferenceToolset::Echo(TEXT("not-json"));

	TSharedPtr<FJsonObject> RejectRoot;

	const TSharedRef<TJsonReader<>> RejectReader = TJsonReaderFactory<>::Create(Rejected);

	TestTrue(TEXT("reject parseable"),

		FJsonSerializer::Deserialize(RejectReader, RejectRoot) && RejectRoot.IsValid());

	if (RejectRoot.IsValid())

	{

		FString RejectStatus;

		RejectRoot->TryGetStringField(TEXT("status"), RejectStatus);

		TestEqual(TEXT("malformed -> rejected"), RejectStatus, FString(TEXT("rejected")));

	}

	return true;

}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(

	FUeremcpReferenceToolsetRegisterAndSchemaTest,

	"UeremcpCore.ReferenceToolset.RegisterAndCaptureSchema",

	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)



bool FUeremcpReferenceToolsetRegisterAndSchemaTest::RunTest(const FString& Parameters)

{

	if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpReferenceToolset::StaticClass()))

	{

		UToolsetRegistry::RegisterToolsetClass(UUeremcpReferenceToolset::StaticClass());

	}



	TestTrue(TEXT("toolset class registered"),

		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpReferenceToolset::StaticClass()));



	// Public schema API

	// [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:52 GetToolsetJsonSchema]

	const FString SchemaJson =

		UToolsetRegistry::GetToolsetJsonSchema(UUeremcpReferenceToolset::StaticClass());

	TestFalse(TEXT("schema non-empty"), SchemaJson.IsEmpty());



	const FString OutPath = FPaths::Combine(

		FPaths::ProjectPluginsDir(),

		TEXT("UEREMCP"),

		TEXT("Saved"),

		TEXT("rb03_echo_tool_schema.json"));

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);

	const bool bWrote = FFileHelper::SaveStringToFile(

		SchemaJson, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	TestTrue(TEXT("wrote schema capture file"), bWrote);



	AddInfo(FString::Printf(TEXT("Captured toolset schema (%d chars) to %s"),

		SchemaJson.Len(), *OutPath));



	TSharedPtr<FJsonObject> SchemaRoot;

	const TSharedRef<TJsonReader<>> SchemaReader = TJsonReaderFactory<>::Create(SchemaJson);

	TestTrue(TEXT("schema JSON parses"),

		FJsonSerializer::Deserialize(SchemaReader, SchemaRoot) && SchemaRoot.IsValid());

	if (!SchemaRoot.IsValid())

	{

		return false;

	}



	const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;

	TestTrue(TEXT("tools array"), SchemaRoot->TryGetArrayField(TEXT("tools"), Tools) && Tools);

	bool bFoundEchoParam = false;

	if (Tools)

	{

		for (const TSharedPtr<FJsonValue>& ToolVal : *Tools)

		{

			const TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();

			if (!ToolObj.IsValid())

			{

				continue;

			}

			FString ToolName;

			ToolObj->TryGetStringField(TEXT("name"), ToolName);

			if (!ToolName.Contains(TEXT("Echo")))

			{

				continue;

			}

			const TSharedPtr<FJsonObject>* InputSchema = nullptr;

			if (!ToolObj->TryGetObjectField(TEXT("inputSchema"), InputSchema) || !InputSchema)

			{

				continue;

			}

			const TSharedPtr<FJsonObject>* Properties = nullptr;

			if (!(*InputSchema)->TryGetObjectField(TEXT("properties"), Properties) || !Properties)

			{

				continue;

			}

			// UHT JSON naming: RequestJson -> requestJson

			// [VERIFIED: Epic FakeToolset InString -> inString

			//  in FunctionLibraryToolsetTest.cpp]

			const TSharedPtr<FJsonObject>* ParamSchema = nullptr;

			if ((*Properties)->TryGetObjectField(TEXT("requestJson"), ParamSchema) && ParamSchema)

			{

				bFoundEchoParam = true;

				FString ParamType;

				(*ParamSchema)->TryGetStringField(TEXT("type"), ParamType);

				TestEqual(TEXT("requestJson type is string"), ParamType, FString(TEXT("string")));



				FString ParamOnly;

				const TSharedRef<TJsonWriter<>> W =

					TJsonWriterFactory<>::Create(&ParamOnly);

				FJsonSerializer::Serialize((*ParamSchema).ToSharedRef(), W);

				AddInfo(FString::Printf(

					TEXT("RB-03 q6 VERBATIM requestJson schema: %s"), *ParamOnly));



				const FString ParamPath = FPaths::Combine(

					FPaths::ProjectPluginsDir(),

					TEXT("UEREMCP"),

					TEXT("Saved"),

					TEXT("rb03_q6_requestJson_schema.json"));

				FFileHelper::SaveStringToFile(

					ParamOnly, *ParamPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

			}

		}

	}

	TestTrue(TEXT("found Echo requestJson property schema"), bFoundEchoParam);

	return true;

}



#endif // WITH_DEV_AUTOMATION_TESTS


