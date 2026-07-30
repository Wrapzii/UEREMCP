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

#include "UeremcpEnvelope.h"
#include "UeremcpJobRegistry.h"
#include "UeremcpReferenceToolset.h"



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

	FString ProtocolVersion;

	TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Status));

	TestEqual(TEXT("status is no_change_required"), Status, FString(TEXT("no_change_required")));

	TestTrue(

		TEXT("protocol_version present"),

		Root->TryGetStringField(TEXT("protocol_version"), ProtocolVersion));

	TestEqual(

		TEXT("protocol_version matches schema"),

		ProtocolVersion,

		FUeremcpEnvelope::ProtocolVersion());



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

		R"({"protocol_version":"1.0","request_id":"echo-1","action":"reference_echo","target":{"asset_path":"/Game/None"}})");

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
	FUeremcpReferenceToolsetExecutePlanTest,
	"UeremcpCore.ReferenceToolset.ExecutePlan",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpReferenceToolsetExecutePlanTest::RunTest(const FString& Parameters)
{
	const FString Json = UUeremcpReferenceToolset::ExecutePlan(TEXT("not-json"));

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(
		TEXT("ExecutePlan delegates malformed input to a parseable rejection"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("malformed execute_plan request is rejected"), Status, FString(TEXT("rejected")));
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
	bool bFoundExecutePlanParam = false;

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

			const bool bIsEcho = ToolName.Contains(TEXT("Echo"));
			const bool bIsExecutePlan = ToolName.Contains(TEXT("ExecutePlan"));
			if (!bIsEcho && !bIsExecutePlan)

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

				bFoundEchoParam |= bIsEcho;
				bFoundExecutePlanParam |= bIsExecutePlan;

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
	TestTrue(TEXT("found ExecutePlan requestJson property schema"), bFoundExecutePlanParam);

	return true;

}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(

	FUeremcpReferenceToolsetGetJobResultTest,

	"UeremcpCore.ReferenceToolset.GetJobResult",

	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)



bool FUeremcpReferenceToolsetGetJobResultTest::RunTest(const FString& Parameters)

{

	FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();

	Registry.Clear();

	FString Error;

	FString JobId;

	TestTrue(

		TEXT("shared-registry job created"),

		Registry.CreateJob(TEXT("origin-request"), false, TEXT("Working"), JobId, Error));

	TestTrue(TEXT("shared-registry job starts"), Registry.StartJob(JobId, Error));



	const FString PollRequest = FString::Printf(

		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"poll-request\","

			"\"action\":\"get_job_result\",\"specification\":{\"job_id\":\"%s\"}}"),

		*JobId);

	const FString PollJson = UUeremcpReferenceToolset::GetJobResult(PollRequest);



	TSharedPtr<FJsonObject> PollRoot;

	const TSharedRef<TJsonReader<>> PollReader = TJsonReaderFactory<>::Create(PollJson);

	TestTrue(TEXT("GetJobResult returns parseable JSON"),

		FJsonSerializer::Deserialize(PollReader, PollRoot) && PollRoot.IsValid());

	if (!PollRoot.IsValid())

	{

		Registry.Clear();

		return false;

	}



	FString PollRequestId;

	FString PollStatus;

	PollRoot->TryGetStringField(TEXT("request_id"), PollRequestId);

	PollRoot->TryGetStringField(TEXT("status"), PollStatus);

	TestEqual(TEXT("poll response uses current request id"), PollRequestId, FString(TEXT("poll-request")));

	TestEqual(TEXT("running poll is partial"), PollStatus, FString(TEXT("partially_completed")));



	const TSharedPtr<FJsonObject>* JobObj = nullptr;

	TestTrue(TEXT("job object present"), PollRoot->TryGetObjectField(TEXT("job"), JobObj) && JobObj && JobObj->IsValid());

	if (JobObj && JobObj->IsValid())

	{

		FString ReturnedJobId;

		(*JobObj)->TryGetStringField(TEXT("job_id"), ReturnedJobId);

		TestEqual(TEXT("running poll preserves job id"), ReturnedJobId, JobId);

	}



	const TSharedPtr<FJsonObject>* Metrics = nullptr;

	TestTrue(TEXT("metrics present"), PollRoot->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid());

	if (Metrics && Metrics->IsValid())

	{

		TestEqual(

			TEXT("action poll increments cumulative round trips"),

			static_cast<int32>((*Metrics)->GetNumberField(TEXT("mcp_round_trips"))),

			2);

	}



	Registry.Clear();

	return true;

}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(

	FUeremcpReferenceToolsetCancelJobTest,

	"UeremcpCore.ReferenceToolset.CancelJob",

	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)



bool FUeremcpReferenceToolsetCancelJobTest::RunTest(const FString& Parameters)

{

	FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();

	Registry.Clear();

	FString Error;

	FString JobId;

	bool bCancelCalled = false;

	TestTrue(

		TEXT("cancellable job created"),

		Registry.CreateJob(

			TEXT("origin-request"),

			true,

			TEXT("Working"),

			JobId,

			Error,

			[&bCancelCalled]() { bCancelCalled = true; return true; }));

	TestTrue(TEXT("cancellable job starts"), Registry.StartJob(JobId, Error));



	const FString CancelRequest = FString::Printf(

		TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"cancel-request\","

			"\"action\":\"cancel_job\",\"specification\":{\"job_id\":\"%s\"}}"),

		*JobId);

	const FString CancelJson = UUeremcpReferenceToolset::CancelJob(CancelRequest);



	TSharedPtr<FJsonObject> CancelRoot;

	const TSharedRef<TJsonReader<>> CancelReader = TJsonReaderFactory<>::Create(CancelJson);

	TestTrue(TEXT("CancelJob returns parseable JSON"),

		FJsonSerializer::Deserialize(CancelReader, CancelRoot) && CancelRoot.IsValid());

	if (!CancelRoot.IsValid())

	{

		Registry.Clear();

		return false;

	}



	FString CancelRequestId;

	CancelRoot->TryGetStringField(TEXT("request_id"), CancelRequestId);

	TestEqual(TEXT("cancel response uses current request id"), CancelRequestId, FString(TEXT("cancel-request")));

	TestTrue(TEXT("cooperative callback invoked"), bCancelCalled);



	const TSharedPtr<FJsonObject>* JobObj = nullptr;

	if (CancelRoot->TryGetObjectField(TEXT("job"), JobObj) && JobObj && JobObj->IsValid())

	{

		FString JobState;

		(*JobObj)->TryGetStringField(TEXT("state"), JobState);

		TestEqual(TEXT("cancelled state returned"), JobState, FString(TEXT("cancelled")));

	}



	Registry.Clear();

	return true;

}



#endif // WITH_DEV_AUTOMATION_TESTS


