#include "UeremcpJobConstraints.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

EUeremcpJobDispatchModel UeremcpTransport::ResolveDispatchModel(const int32 TimeoutMs)
{
	if (TimeoutMs <= 0)
	{
		return EUeremcpJobDispatchModel::InlineComplete;
	}
	return EUeremcpJobDispatchModel::PollAfterTimeout;
}

FUeremcpTransportCapabilityFlags UeremcpTransport::GetStaticCapabilityFlags()
{
	FUeremcpTransportCapabilityFlags Flags;
	// Epic serves MCP over HTTP with SSE tool streams
	// [VERIFIED: ModelContextProtocolServer.h:23, ModelContextProtocolServer.cpp:416-447]
	Flags.bHttpTransportOnly = true;
	Flags.bStreamableHttpSse = true;
	// No stdio references anywhere in the plugin
	// [VERIFIED: grep ModelContextProtocol — zero stdio matches]
	Flags.bStdioTransport = false;
	// resources/list + resources/read + IModelContextProtocolResourceProvider
	// [VERIFIED: ModelContextProtocolServer.cpp:35-36, IModelContextProtocolResourceProvider.h:20]
	Flags.bMcpResourcesSupported = true;
	// notifications/progress on active SSE streams when progressToken set
	// [VERIFIED: ModelContextProtocolServer.cpp:283-307, 1036-1062]
	Flags.bMcpProgressNotifications = true;
	// notifications/cancelled -> IModelContextProtocolTool::CancelAsync
	// [VERIFIED: ModelContextProtocolServer.cpp:697-728]
	Flags.bMcpCancellationNotification = true;
	// FToolsetRegistryToolAdapter does not override CancelAsync
	// [VERIFIED: ModelContextProtocolToolsetRegistryAdapter.h:13-26]
	Flags.bToolsetRegistryCancelWired = false;
	// GET returns 405; no persistent push channel outside in-flight tools/call
	// [VERIFIED: ModelContextProtocolServer.cpp:1012-1014, 1066-1075]
	Flags.bPersistentServerPushChannel = false;
	// No engine job-id registry; only JSON-RPC request ids per session
	// [VERIFIED: ModelContextProtocolSession.h:96-138]
	Flags.bEngineJobIds = false;
	// Origin header localhost guard only; no tokens
	// [VERIFIED: ModelContextProtocolServer.cpp:68-120]
	Flags.bEngineAuth = false;
	Flags.bOriginLocalhostGuard = true;
	return Flags;
}

FString UeremcpTransport::CapabilityFlagsToJson(const FUeremcpTransportCapabilityFlags& Flags)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> CapObj = MakeShared<FJsonObject>();
	CapObj->SetBoolField(TEXT("http_transport_only"), Flags.bHttpTransportOnly);
	CapObj->SetBoolField(TEXT("streamable_http_sse"), Flags.bStreamableHttpSse);
	CapObj->SetBoolField(TEXT("stdio_transport"), Flags.bStdioTransport);
	CapObj->SetBoolField(TEXT("mcp_resources"), Flags.bMcpResourcesSupported);
	CapObj->SetBoolField(TEXT("mcp_progress_notifications"), Flags.bMcpProgressNotifications);
	CapObj->SetBoolField(TEXT("mcp_cancellation_notification"), Flags.bMcpCancellationNotification);
	CapObj->SetBoolField(TEXT("toolset_registry_cancel_wired"), Flags.bToolsetRegistryCancelWired);
	CapObj->SetBoolField(TEXT("persistent_server_push"), Flags.bPersistentServerPushChannel);
	CapObj->SetBoolField(TEXT("engine_job_ids"), Flags.bEngineJobIds);
	CapObj->SetBoolField(TEXT("engine_auth"), Flags.bEngineAuth);
	CapObj->SetBoolField(TEXT("origin_localhost_guard"), Flags.bOriginLocalhostGuard);
	Root->SetObjectField(TEXT("capabilities"), CapObj);

	const TSharedRef<FJsonObject> DefaultsObj = MakeShared<FJsonObject>();
	DefaultsObj->SetNumberField(TEXT("default_timeout_ms"), FUeremcpJobModelDefaults::DefaultTimeoutMs);
	DefaultsObj->SetNumberField(TEXT("min_timeout_ms"), FUeremcpJobModelDefaults::MinTimeoutMs);
	DefaultsObj->SetNumberField(TEXT("max_timeout_ms"), FUeremcpJobModelDefaults::MaxTimeoutMs);
	DefaultsObj->SetStringField(TEXT("poll_action"), FUeremcpJobModelDefaults::PollActionName);
	Root->SetObjectField(TEXT("job_defaults"), DefaultsObj);

	Root->SetStringField(TEXT("recommended_dispatch"),
		ResolveDispatchModel(0) == EUeremcpJobDispatchModel::InlineComplete
			? TEXT("inline_complete")
			: TEXT("poll_after_timeout"));
	Root->SetStringField(TEXT("handoff_version"), TEXT("ws04-wave1-1"));

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
