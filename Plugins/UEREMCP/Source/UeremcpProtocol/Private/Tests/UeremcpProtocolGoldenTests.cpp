// Golden-vector parity tests for UeremcpProtocol (WS-14 C-2).
//
// Loads the same Tests/golden/ fixtures as the Python suite and asserts
// FUeremcp* PRODUCTION code matches. Python-only green is not parity.
//
// Run (editor / commandlet — requires the plugin built into a uproject):
//   UnrealEditor-Cmd.exe <Project>.uproject -unattended -NullRHI -nop4
//     -ExecCmds="Automation RunTests UEREMCP.Protocol.Golden;Quit"
//
// Golden root resolution order:
//   1) env UEREMCP_PROTOCOL_GOLDEN_ROOT
//   2) <ProjectDir>/Plugins/UEREMCP/Source/UeremcpProtocol/Tests/golden
//   3) <ProjectPluginsDir>/UEREMCP/Source/UeremcpProtocol/Tests/golden
//
// See Docs/CPP_PARITY.md.

#include "UeremcpContentHash.h"
#include "UeremcpDependencyOrder.h"
#include "UeremcpEnvelope.h"
#include "UeremcpRefResolve.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpProtocolGolden
{
	static FString GoldenRoot()
	{
		const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("UEREMCP_PROTOCOL_GOLDEN_ROOT"));
		if (!Env.IsEmpty() && FPaths::DirectoryExists(Env))
		{
			return FPaths::ConvertRelativePathToFull(Env);
		}

		const TArray<FString> Candidates = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/UEREMCP/Source/UeremcpProtocol/Tests/golden")),
			FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UEREMCP/Source/UeremcpProtocol/Tests/golden")),
		};
		for (const FString& Candidate : Candidates)
		{
			if (FPaths::DirectoryExists(Candidate))
			{
				return FPaths::ConvertRelativePathToFull(Candidate);
			}
		}
		return FString();
	}

	static bool ReadTextFile(const FString& AbsolutePath, FString& Out, FAutomationTestBase& Test)
	{
		if (!FFileHelper::LoadFileToString(Out, *AbsolutePath))
		{
			Test.AddError(FString::Printf(TEXT("Failed to read golden file: %s"), *AbsolutePath));
			return false;
		}
		Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		return true;
	}

	static bool ReadJsonFile(const FString& AbsolutePath, TSharedPtr<FJsonValue>& Out, FAutomationTestBase& Test)
	{
		FString Text;
		if (!ReadTextFile(AbsolutePath, Text, Test))
		{
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("Invalid JSON golden: %s"), *AbsolutePath));
			return false;
		}
		return true;
	}

	static bool ReadJsonObjectFile(const FString& AbsolutePath, TSharedPtr<FJsonObject>& Out, FAutomationTestBase& Test)
	{
		TSharedPtr<FJsonValue> Value;
		if (!ReadJsonFile(AbsolutePath, Value, Test))
		{
			return false;
		}
		if (Value->Type != EJson::Object)
		{
			Test.AddError(FString::Printf(TEXT("Expected JSON object: %s"), *AbsolutePath));
			return false;
		}
		Out = Value->AsObject();
		return true;
	}

	static bool JsonEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (!A.IsValid() || !B.IsValid())
		{
			return A.IsValid() == B.IsValid();
		}
		if (A->Type != B->Type)
		{
			// Number vs number: Unreal may parse integers as numbers; treat numeric equality.
			if ((A->Type == EJson::Number || A->Type == EJson::Boolean) == false
				|| (B->Type == EJson::Number || B->Type == EJson::Boolean) == false)
			{
				if (!(A->Type == EJson::Number && B->Type == EJson::Number))
				{
					return false;
				}
			}
		}
		switch (A->Type)
		{
		case EJson::Null:
			return B->Type == EJson::Null;
		case EJson::Boolean:
			return B->Type == EJson::Boolean && A->AsBool() == B->AsBool();
		case EJson::Number:
			return B->Type == EJson::Number && A->AsNumber() == B->AsNumber();
		case EJson::String:
			return B->Type == EJson::String && A->AsString() == B->AsString();
		case EJson::Array:
		{
			if (B->Type != EJson::Array)
			{
				return false;
			}
			const TArray<TSharedPtr<FJsonValue>>& AA = A->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& BB = B->AsArray();
			if (AA.Num() != BB.Num())
			{
				return false;
			}
			for (int32 I = 0; I < AA.Num(); ++I)
			{
				if (!JsonEqual(AA[I], BB[I]))
				{
					return false;
				}
			}
			return true;
		}
		case EJson::Object:
		{
			if (B->Type != EJson::Object)
			{
				return false;
			}
			const TSharedPtr<FJsonObject> OA = A->AsObject();
			const TSharedPtr<FJsonObject> OB = B->AsObject();
			if (OA->Values.Num() != OB->Values.Num())
			{
				return false;
			}
			for (const auto& Pair : OA->Values)
			{
				if (!OB->HasField(Pair.Key) || !JsonEqual(Pair.Value, OB->TryGetField(Pair.Key)))
				{
					return false;
				}
			}
			return true;
		}
		default:
			return false;
		}
	}

	static FString PathJoin(const FString& Root, const TCHAR* Rel)
	{
		return FPaths::Combine(Root, Rel);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpProtocolGoldenContentHash,
	"UEREMCP.Protocol.Golden.ContentHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpProtocolGoldenContentHash::RunTest(const FString& Parameters)
{
	using namespace UeremcpProtocolGolden;
	const FString Root = GoldenRoot();
	if (Root.IsEmpty())
	{
		AddError(TEXT("Golden root not found. Set UEREMCP_PROTOCOL_GOLDEN_ROOT or install plugin under Project/Plugins/UEREMCP."));
		return false;
	}

	FString GraphText;
	if (!ReadTextFile(PathJoin(Root, TEXT("content_hash/graph.in.json")), GraphText, *this))
	{
		return false;
	}
	FString Expected;
	if (!ReadTextFile(PathJoin(Root, TEXT("content_hash/hash.expected.txt")), Expected, *this))
	{
		return false;
	}
	Expected.TrimStartAndEndInline();

	FString Error;
	const FString Got = FUeremcpContentHash::HashJsonString(GraphText, &Error);
	TestTrue(FString::Printf(TEXT("HashJsonString ok: %s"), *Error), !Got.IsEmpty());
	TestEqual(TEXT("content_hash matches golden"), Got, Expected);

	FString CosmeticText;
	if (!ReadTextFile(PathJoin(Root, TEXT("content_hash/graph_cosmetic.in.json")), CosmeticText, *this))
	{
		return false;
	}
	const FString CosmeticHash = FUeremcpContentHash::HashJsonString(CosmeticText, &Error);
	TestEqual(TEXT("cosmetic graph same content_hash"), CosmeticHash, Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpProtocolGoldenEnvelope,
	"UEREMCP.Protocol.Golden.Envelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpProtocolGoldenEnvelope::RunTest(const FString& Parameters)
{
	using namespace UeremcpProtocolGolden;
	const FString Root = GoldenRoot();
	if (Root.IsEmpty())
	{
		AddError(TEXT("Golden root not found."));
		return false;
	}

	FString RequestText;
	if (!ReadTextFile(PathJoin(Root, TEXT("envelope/request.in.json")), RequestText, *this))
	{
		return false;
	}
	FUeremcpRequest Request;
	FString ParseError;
	TestTrue(FString::Printf(TEXT("ParseRequest: %s"), *ParseError),
		FUeremcpEnvelope::ParseRequest(RequestText, Request, ParseError));

	TSharedPtr<FJsonObject> ExpectedParsed;
	if (!ReadJsonObjectFile(PathJoin(Root, TEXT("envelope/request.parsed.expected.json")), ExpectedParsed, *this))
	{
		return false;
	}

	TestEqual(TEXT("protocol_version"), Request.ProtocolVersion, ExpectedParsed->GetStringField(TEXT("protocol_version")));
	TestEqual(TEXT("request_id"), Request.RequestId, ExpectedParsed->GetStringField(TEXT("request_id")));
	TestEqual(TEXT("action"), Request.Action, ExpectedParsed->GetStringField(TEXT("action")));
	TestEqual(TEXT("mode"), Request.Mode, ExpectedParsed->GetStringField(TEXT("mode")));
	TestEqual(TEXT("target_asset_path"), Request.TargetAssetPath, ExpectedParsed->GetStringField(TEXT("target_asset_path")));
	TestEqual(TEXT("engine_version"), Request.EngineVersion, ExpectedParsed->GetStringField(TEXT("engine_version")));
	TestEqual(TEXT("dry_run"), Request.bDryRun, ExpectedParsed->GetBoolField(TEXT("dry_run")));
	TestEqual(TEXT("atomic"), Request.bAtomic, ExpectedParsed->GetBoolField(TEXT("atomic")));
	TestEqual(
		TEXT("allow_destructive"),
		Request.bAllowDestructive,
		ExpectedParsed->GetBoolField(TEXT("allow_destructive")));
	TestEqual(TEXT("response_detail"), Request.ResponseDetail, ExpectedParsed->GetStringField(TEXT("response_detail")));
	TestEqual(TEXT("timeout_ms"), Request.TimeoutMs, static_cast<int32>(ExpectedParsed->GetNumberField(TEXT("timeout_ms"))));
	TestEqual(TEXT("has_expected_revision"), Request.bHasExpectedRevision, ExpectedParsed->GetBoolField(TEXT("has_expected_revision")));
	TestEqual(TEXT("idempotency_key"), Request.IdempotencyKey, ExpectedParsed->GetStringField(TEXT("idempotency_key")));
	FString SpecName;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("name"), SpecName);
	}
	TestEqual(TEXT("specification_name"), SpecName, ExpectedParsed->GetStringField(TEXT("specification_name")));

	TSharedPtr<FJsonObject> Fields;
	if (!ReadJsonObjectFile(PathJoin(Root, TEXT("envelope/response_fields.in.json")), Fields, *this))
	{
		return false;
	}
	FUeremcpResponse Response;
	Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Response.RequestId = Fields->GetStringField(TEXT("request_id"));
	Response.Status = Fields->GetStringField(TEXT("status"));
	Response.Summary = Fields->GetStringField(TEXT("summary"));
	Fields->TryGetStringField(TEXT("understood_action"), Response.UnderstoodAction);
	Fields->TryGetStringField(TEXT("understood_target"), Response.UnderstoodTarget);
	Fields->TryGetStringField(TEXT("primary_asset"), Response.PrimaryAsset);
	if (Fields->HasTypedField<EJson::Object>(TEXT("metrics")))
	{
		const TSharedPtr<FJsonObject> Metrics = Fields->GetObjectField(TEXT("metrics"));
		Response.Metrics.McpRoundTrips = static_cast<int32>(Metrics->GetNumberField(TEXT("mcp_round_trips")));
		Response.Metrics.InternalOperations = static_cast<int32>(Metrics->GetNumberField(TEXT("internal_operations")));
	}

	const FString Serialized = FUeremcpEnvelope::SerializeResponse(Response);
	TSharedPtr<FJsonValue> GotValue;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
		TestTrue(TEXT("SerializeResponse produces JSON"), FJsonSerializer::Deserialize(Reader, GotValue) && GotValue.IsValid());
	}
	TSharedPtr<FJsonValue> ExpectedValue;
	if (!ReadJsonFile(PathJoin(Root, TEXT("envelope/response.out.expected.json")), ExpectedValue, *this))
	{
		return false;
	}
	TestTrue(TEXT("serialized response matches golden (semantic JSON)"), JsonEqual(GotValue, ExpectedValue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpProtocolGoldenRef,
	"UEREMCP.Protocol.Golden.Ref",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpProtocolGoldenRef::RunTest(const FString& Parameters)
{
	using namespace UeremcpProtocolGolden;
	const FString Root = GoldenRoot();
	if (Root.IsEmpty())
	{
		AddError(TEXT("Golden root not found."));
		return false;
	}

	TSharedPtr<FJsonValue> SpecValue;
	if (!ReadJsonFile(PathJoin(Root, TEXT("ref/spec.in.json")), SpecValue, *this))
	{
		return false;
	}
	TSharedPtr<FJsonObject> CompletedRoot;
	if (!ReadJsonObjectFile(PathJoin(Root, TEXT("ref/completed.in.json")), CompletedRoot, *this))
	{
		return false;
	}

	TMap<FString, TSharedPtr<FJsonObject>> Completed;
	for (const auto& Pair : CompletedRoot->Values)
	{
		if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Object)
		{
			Completed.Add(FString(Pair.Key), Pair.Value->AsObject());
		}
	}

	FString Error;
	TestTrue(FString::Printf(TEXT("ResolveInPlace: %s"), *Error),
		FUeremcpRefResolve::ResolveInPlace(SpecValue, Completed, Error));

	TSharedPtr<FJsonValue> ExpectedValue;
	if (!ReadJsonFile(PathJoin(Root, TEXT("ref/resolved.expected.json")), ExpectedValue, *this))
	{
		return false;
	}
	TestTrue(TEXT("$ref resolution matches golden"), JsonEqual(SpecValue, ExpectedValue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpProtocolGoldenTopo,
	"UEREMCP.Protocol.Golden.Topo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpProtocolGoldenTopo::RunTest(const FString& Parameters)
{
	using namespace UeremcpProtocolGolden;
	const FString Root = GoldenRoot();
	if (Root.IsEmpty())
	{
		AddError(TEXT("Golden root not found."));
		return false;
	}

	TSharedPtr<FJsonValue> NodesValue;
	if (!ReadJsonFile(PathJoin(Root, TEXT("topo/nodes.in.json")), NodesValue, *this))
	{
		return false;
	}
	TestTrue(TEXT("nodes.in.json is array"), NodesValue->Type == EJson::Array);

	TArray<FUeremcpDependencyNode> Nodes;
	for (const TSharedPtr<FJsonValue>& Item : NodesValue->AsArray())
	{
		TestTrue(TEXT("node is object"), Item.IsValid() && Item->Type == EJson::Object);
		const TSharedPtr<FJsonObject> Obj = Item->AsObject();
		FUeremcpDependencyNode Node;
		Node.Id = Obj->GetStringField(TEXT("id"));
		const TArray<TSharedPtr<FJsonValue>>* Deps = nullptr;
		if (Obj->TryGetArrayField(TEXT("depends_on"), Deps) && Deps)
		{
			for (const TSharedPtr<FJsonValue>& Dep : *Deps)
			{
				Node.DependsOn.Add(Dep->AsString());
			}
		}
		Nodes.Add(Node);
	}

	TArray<FString> Ordered;
	FString Error;
	TestTrue(FString::Printf(TEXT("TopologicalSort: %s"), *Error),
		FUeremcpDependencyOrder::TopologicalSort(Nodes, Ordered, Error));

	TSharedPtr<FJsonValue> ExpectedValue;
	if (!ReadJsonFile(PathJoin(Root, TEXT("topo/order.expected.json")), ExpectedValue, *this))
	{
		return false;
	}
	TestTrue(TEXT("order.expected.json is array"), ExpectedValue->Type == EJson::Array);
	const TArray<TSharedPtr<FJsonValue>>& ExpectedArr = ExpectedValue->AsArray();
	TestEqual(TEXT("topo length"), Ordered.Num(), ExpectedArr.Num());
	for (int32 I = 0; I < Ordered.Num() && I < ExpectedArr.Num(); ++I)
	{
		TestEqual(
			FString::Printf(TEXT("topo[%d]"), I),
			Ordered[I],
			ExpectedArr[I]->AsString());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
