#include "UeremcpIdempotency.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	static FString AnnotateReplayedResponse(const FString& StoredJson, const FString& RequestId)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StoredJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return StoredJson;
		}

		if (!RequestId.IsEmpty())
		{
			Root->SetStringField(TEXT("request_id"), RequestId);
		}

		const TSharedPtr<FJsonObject>* MetricsField = nullptr;
		TSharedPtr<FJsonObject> Metrics;
		if (Root->TryGetObjectField(TEXT("metrics"), MetricsField) && MetricsField && MetricsField->IsValid())
		{
			Metrics = *MetricsField;
		}
		else
		{
			Metrics = MakeShared<FJsonObject>();
			Root->SetObjectField(TEXT("metrics"), Metrics);
		}
		Metrics->SetBoolField(TEXT("replayed"), true);

		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Out;
	}
}

FUeremcpIdempotencyStore& FUeremcpIdempotencyStore::Get()
{
	static FUeremcpIdempotencyStore Instance;
	return Instance;
}

bool FUeremcpIdempotencyStore::TryGet(const FString& Key, FString& OutResponseJson) const
{
	if (Key.IsEmpty())
	{
		return false;
	}
	if (const FString* Found = Entries.Find(Key))
	{
		OutResponseJson = *Found;
		return true;
	}
	return false;
}

bool FUeremcpIdempotencyStore::TryGetReplay(
	const FString& Key,
	const FString& RequestId,
	FString& OutResponseJson) const
{
	FString Stored;
	if (!TryGet(Key, Stored))
	{
		return false;
	}
	OutResponseJson = AnnotateReplayedResponse(Stored, RequestId);
	return true;
}

void FUeremcpIdempotencyStore::Put(const FString& Key, const FString& ResponseJson)
{
	if (Key.IsEmpty())
	{
		return;
	}
	Entries.Add(Key, ResponseJson);
}

void FUeremcpIdempotencyStore::Clear()
{
	Entries.Empty();
}
