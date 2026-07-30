#include "UeremcpJobConstraints.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	static bool ReadBoolField(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldName,
		bool& OutValue,
		FString& OutError)
	{
		if (!Object->HasTypedField<EJson::Boolean>(FieldName))
		{
			OutError = FString::Printf(TEXT("capabilities.%s missing or not bool"), *FieldName);
			return false;
		}
		OutValue = Object->GetBoolField(FieldName);
		return true;
	}

	static bool ReadIntField(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldName,
		int32& OutValue,
		FString& OutError)
	{
		if (!Object->HasTypedField<EJson::Number>(FieldName))
		{
			OutError = FString::Printf(TEXT("job_defaults.%s missing or not number"), *FieldName);
			return false;
		}
		OutValue = static_cast<int32>(Object->GetNumberField(FieldName));
		return true;
	}

	static bool CapabilityFlagsEqual(
		const FUeremcpTransportCapabilityFlags& Left,
		const FUeremcpTransportCapabilityFlags& Right)
	{
		return Left.bHttpTransportOnly == Right.bHttpTransportOnly
			&& Left.bStreamableHttpSse == Right.bStreamableHttpSse
			&& Left.bStdioTransport == Right.bStdioTransport
			&& Left.bMcpResourcesSupported == Right.bMcpResourcesSupported
			&& Left.bMcpProgressNotifications == Right.bMcpProgressNotifications
			&& Left.bMcpCancellationNotification == Right.bMcpCancellationNotification
			&& Left.bToolsetRegistryCancelWired == Right.bToolsetRegistryCancelWired
			&& Left.bPersistentServerPushChannel == Right.bPersistentServerPushChannel
			&& Left.bEngineJobIds == Right.bEngineJobIds
			&& Left.bEngineAuth == Right.bEngineAuth
			&& Left.bOriginLocalhostGuard == Right.bOriginLocalhostGuard;
	}
}

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

FString UeremcpTransport::ResolveHandoffJsonPath()
{
	const TCHAR* Relative = TEXT("Source/UeremcpTransport/constraints/transport_job_handoff.json");

	const auto TryCandidate = [](const FString& Candidate) -> FString
	{
		if (FPaths::FileExists(Candidate))
		{
			return FPaths::ConvertRelativePathToFull(Candidate);
		}
		return FString();
	};

	for (const FString& PluginName : { TEXT("UEREMCP"), TEXT("UEREMCPTransportTest") })
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
		{
			const FString Resolved = TryCandidate(FPaths::Combine(Plugin->GetBaseDir(), Relative));
			if (!Resolved.IsEmpty())
			{
				return Resolved;
			}
		}
	}

	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		const FString Resolved = TryCandidate(FPaths::Combine(Plugin->GetBaseDir(), Relative));
		if (!Resolved.IsEmpty())
		{
			return Resolved;
		}
	}

	const TArray<FString> Candidates = {
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UEREMCP"), Relative),
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UEREMCPTransportTest"), Relative),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/UEREMCP"), Relative),
	};
	for (const FString& Candidate : Candidates)
	{
		const FString Resolved = TryCandidate(Candidate);
		if (!Resolved.IsEmpty())
		{
			return Resolved;
		}
	}
	return FString();
}

bool UeremcpTransport::ParseHandoffConstraintsJson(
	const FString& JsonText,
	FUeremcpHandoffConstraints& OutHandoff,
	FString& OutError)
{
	OutError.Reset();
	OutHandoff = FUeremcpHandoffConstraints();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("handoff JSON is not a valid object");
		return false;
	}

	if (!Root->TryGetStringField(TEXT("handoff_version"), OutHandoff.HandoffVersion)
		|| OutHandoff.HandoffVersion.IsEmpty())
	{
		OutError = TEXT("handoff_version missing or empty");
		return false;
	}

	Root->TryGetStringField(TEXT("recommended_job_model"), OutHandoff.RecommendedJobModel);

	const TSharedPtr<FJsonObject>* CapabilitiesObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("capabilities"), CapabilitiesObj) || !CapabilitiesObj || !(*CapabilitiesObj).IsValid())
	{
		OutError = TEXT("capabilities object missing");
		return false;
	}

	const TSharedPtr<FJsonObject>& Caps = *CapabilitiesObj;
	if (!ReadBoolField(Caps, TEXT("http_transport_only"), OutHandoff.Capabilities.bHttpTransportOnly, OutError)
		|| !ReadBoolField(Caps, TEXT("streamable_http_sse"), OutHandoff.Capabilities.bStreamableHttpSse, OutError)
		|| !ReadBoolField(Caps, TEXT("stdio_transport"), OutHandoff.Capabilities.bStdioTransport, OutError)
		|| !ReadBoolField(Caps, TEXT("mcp_resources"), OutHandoff.Capabilities.bMcpResourcesSupported, OutError)
		|| !ReadBoolField(Caps, TEXT("mcp_progress_notifications"), OutHandoff.Capabilities.bMcpProgressNotifications, OutError)
		|| !ReadBoolField(Caps, TEXT("mcp_cancellation_notification"), OutHandoff.Capabilities.bMcpCancellationNotification, OutError)
		|| !ReadBoolField(Caps, TEXT("toolset_registry_cancel_wired"), OutHandoff.Capabilities.bToolsetRegistryCancelWired, OutError)
		|| !ReadBoolField(Caps, TEXT("persistent_server_push"), OutHandoff.Capabilities.bPersistentServerPushChannel, OutError)
		|| !ReadBoolField(Caps, TEXT("engine_job_ids"), OutHandoff.Capabilities.bEngineJobIds, OutError)
		|| !ReadBoolField(Caps, TEXT("engine_auth"), OutHandoff.Capabilities.bEngineAuth, OutError)
		|| !ReadBoolField(Caps, TEXT("origin_localhost_guard"), OutHandoff.Capabilities.bOriginLocalhostGuard, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* DefaultsObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("job_defaults"), DefaultsObj) || !DefaultsObj || !(*DefaultsObj).IsValid())
	{
		OutError = TEXT("job_defaults object missing");
		return false;
	}

	const TSharedPtr<FJsonObject>& Defaults = *DefaultsObj;
	if (!ReadIntField(Defaults, TEXT("default_timeout_ms"), OutHandoff.DefaultTimeoutMs, OutError)
		|| !ReadIntField(Defaults, TEXT("min_timeout_ms"), OutHandoff.MinTimeoutMs, OutError)
		|| !ReadIntField(Defaults, TEXT("max_timeout_ms"), OutHandoff.MaxTimeoutMs, OutError))
	{
		return false;
	}

	if (!Defaults->TryGetStringField(TEXT("poll_action"), OutHandoff.PollAction) || OutHandoff.PollAction.IsEmpty())
	{
		OutError = TEXT("job_defaults.poll_action missing or empty");
		return false;
	}

	const TSharedPtr<FJsonObject>* Ws05Obj = nullptr;
	if (!Root->TryGetObjectField(TEXT("ws05_constraints"), Ws05Obj) || !Ws05Obj || !(*Ws05Obj).IsValid())
	{
		OutError = TEXT("ws05_constraints object missing");
		return false;
	}

	const TSharedPtr<FJsonObject>& Ws05 = *Ws05Obj;
	if (!Ws05->HasTypedField<EJson::Boolean>(TEXT("dispatch_inline_when_timeout_ms_zero"))
		|| !Ws05->HasTypedField<EJson::Boolean>(TEXT("dispatch_poll_when_timeout_ms_positive")))
	{
		OutError = TEXT("ws05_constraints dispatch flags missing");
		return false;
	}

	OutHandoff.bDispatchInlineWhenTimeoutZero =
		Ws05->GetBoolField(TEXT("dispatch_inline_when_timeout_ms_zero"));
	OutHandoff.bDispatchPollWhenTimeoutPositive =
		Ws05->GetBoolField(TEXT("dispatch_poll_when_timeout_ms_positive"));

	return ValidateHandoffConstraints(OutHandoff, OutError);
}

bool UeremcpTransport::LoadHandoffConstraints(
	FUeremcpHandoffConstraints& OutHandoff,
	FString& OutError)
{
	const FString Path = ResolveHandoffJsonPath();
	if (Path.IsEmpty())
	{
		OutError = TEXT("transport_job_handoff.json not found");
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("failed to read handoff file: %s"), *Path);
		return false;
	}

	return ParseHandoffConstraintsJson(JsonText, OutHandoff, OutError);
}

bool UeremcpTransport::ValidateHandoffConstraints(
	const FUeremcpHandoffConstraints& Handoff,
	FString& OutError)
{
	OutError.Reset();

	if (Handoff.Capabilities.bStdioTransport)
	{
		OutError = TEXT("stdio_transport must be false (HTTP-only substrate)");
		return false;
	}

	if (Handoff.Capabilities.bEngineJobIds)
	{
		OutError = TEXT("engine_job_ids must be false (UEREMCP owns job registry)");
		return false;
	}

	if (Handoff.PollAction != FUeremcpJobModelDefaults::PollActionName)
	{
		OutError = TEXT("job_defaults.poll_action must be get_job_result");
		return false;
	}

	if (!Handoff.bDispatchInlineWhenTimeoutZero)
	{
		OutError = TEXT("ws05_constraints.dispatch_inline_when_timeout_ms_zero must be true");
		return false;
	}

	if (!Handoff.bDispatchPollWhenTimeoutPositive)
	{
		OutError = TEXT("ws05_constraints.dispatch_poll_when_timeout_ms_positive must be true");
		return false;
	}

	if (Handoff.DefaultTimeoutMs != FUeremcpJobModelDefaults::DefaultTimeoutMs
		|| Handoff.MinTimeoutMs != FUeremcpJobModelDefaults::MinTimeoutMs
		|| Handoff.MaxTimeoutMs != FUeremcpJobModelDefaults::MaxTimeoutMs)
	{
		OutError = TEXT("job_defaults timeout bounds drift from FUeremcpJobModelDefaults");
		return false;
	}

	return true;
}

bool UeremcpTransport::HandoffMatchesRuntimeCapabilities(
	const FUeremcpHandoffConstraints& Handoff,
	FString& OutError)
{
	OutError.Reset();

	const FUeremcpTransportCapabilityFlags RuntimeFlags = GetStaticCapabilityFlags();
	if (!CapabilityFlagsEqual(Handoff.Capabilities, RuntimeFlags))
	{
		OutError = TEXT("handoff capabilities drift from GetStaticCapabilityFlags()");
		return false;
	}

	if (Handoff.HandoffVersion != TEXT("ws04-wave1-1"))
	{
		OutError = TEXT("unexpected handoff_version — update C++ constants and tests");
		return false;
	}

	if (Handoff.RecommendedJobModel != TEXT("poll_after_timeout"))
	{
		OutError = TEXT("recommended_job_model drift from ResolveDispatchModel policy");
		return false;
	}

	if (ResolveDispatchModel(0) != EUeremcpJobDispatchModel::InlineComplete
		|| ResolveDispatchModel(Handoff.DefaultTimeoutMs) != EUeremcpJobDispatchModel::PollAfterTimeout)
	{
		OutError = TEXT("ResolveDispatchModel no longer matches ws05_constraints");
		return false;
	}

	return ValidateHandoffConstraints(Handoff, OutError);
}

bool UeremcpTransport::IsValidJobState(const FString& State)
{
	static const TArray<FString> States = {
		TEXT("queued"), TEXT("running"), TEXT("completed"), TEXT("failed"), TEXT("cancelled")
	};
	return States.Contains(State);
}

bool UeremcpTransport::IsTerminalJobState(const FString& State)
{
	return State == TEXT("completed") || State == TEXT("failed") || State == TEXT("cancelled");
}

bool UeremcpTransport::IsValidJobStateTransition(
	const FString& FromState,
	const FString& ToState)
{
	if (!IsValidJobState(FromState) || !IsValidJobState(ToState))
	{
		return false;
	}

	if (FromState == ToState)
	{
		return true;
	}

	if (IsTerminalJobState(FromState))
	{
		return false;
	}

	if (FromState == TEXT("queued"))
	{
		return ToState == TEXT("running") || ToState == TEXT("cancelled");
	}

	if (FromState == TEXT("running"))
	{
		return ToState == TEXT("completed") || ToState == TEXT("failed") || ToState == TEXT("cancelled");
	}

	return false;
}
