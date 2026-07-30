// Minimal ADR-0003 envelope helpers owned by WS-03 for the reference toolset.
//
// UeremcpProtocol on ws-05-protocol @ 4ea413c still fails UE 5.8 compile
// (TMap::GetKeys / TSet::Contains / TCondensedJsonPrintPolicy). Do not link it yet.
// See docs/proposals/ws-03-protocol-module-blocker.md.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Ueremcp::MinimalEnvelope
{
	inline const FString& GetProtocolVersion()
	{
		static const FString Version(TEXT("1.0.0"));
		return Version;
	}

	inline FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	inline FString MakeResponse(
		const FString& RequestId,
		const FString& Status,
		const FString& Summary,
		const FString& UnderstoodAction = FString(),
		const FString& UnderstoodTarget = FString())
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("protocol_version"), GetProtocolVersion());
		if (!RequestId.IsEmpty())
		{
			Root->SetStringField(TEXT("request_id"), RequestId);
		}
		Root->SetStringField(TEXT("status"), Status);
		Root->SetStringField(TEXT("summary"), Summary);

		if (!UnderstoodAction.IsEmpty() || !UnderstoodTarget.IsEmpty())
		{
			const TSharedRef<FJsonObject> Understood = MakeShared<FJsonObject>();
			if (!UnderstoodAction.IsEmpty())
			{
				Understood->SetStringField(TEXT("action"), UnderstoodAction);
			}
			if (!UnderstoodTarget.IsEmpty())
			{
				Understood->SetStringField(TEXT("target"), UnderstoodTarget);
			}
			Root->SetObjectField(TEXT("understood"), Understood);
		}

		const TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
		Metrics->SetNumberField(TEXT("mcp_round_trips"), 1);
		Metrics->SetNumberField(TEXT("internal_operations"), 0);
		Root->SetObjectField(TEXT("metrics"), Metrics);

		return SerializeJsonObject(Root);
	}

	inline FString MakeRejection(const FString& RequestId, const FString& Reason)
	{
		return MakeResponse(RequestId, TEXT("rejected"), Reason);
	}

	/** True when Other's major version matches ours (ADR-0003 rule 4). */
	inline bool IsProtocolCompatible(const FString& Other)
	{
		FString OursMajor;
		FString OtherMajor;
		GetProtocolVersion().Split(TEXT("."), &OursMajor, nullptr);
		if (!Other.Split(TEXT("."), &OtherMajor, nullptr))
		{
			OtherMajor = Other;
		}
		return OursMajor == OtherMajor;
	}

	struct FParsedRequest
	{
		FString ProtocolVersion;
		FString RequestId;
		FString Action;
		FString TargetAssetPath;
	};

	inline bool ParseRequest(const FString& Json, FParsedRequest& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("Request is not a JSON object.");
			return false;
		}

		if (!Root->TryGetStringField(TEXT("protocol_version"), Out.ProtocolVersion)
			|| Out.ProtocolVersion.IsEmpty())
		{
			OutError = TEXT("Missing required field 'protocol_version'.");
			return false;
		}

		Root->TryGetStringField(TEXT("request_id"), Out.RequestId);
		Root->TryGetStringField(TEXT("action"), Out.Action);

		const TSharedPtr<FJsonObject>* TargetObj = nullptr;
		if (Root->TryGetObjectField(TEXT("target"), TargetObj) && TargetObj && TargetObj->IsValid())
		{
			(*TargetObj)->TryGetStringField(TEXT("asset_path"), Out.TargetAssetPath);
		}

		return true;
	}
}
