#include "UeremcpIdempotency.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UeremcpSha256.h"

namespace
{
	static constexpr const TCHAR* GRecordVersionField = TEXT("ueremcp_idempotency_record_version");
	static constexpr int32 GRecordVersion = 1;

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

	static bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		OutObject.Reset();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	static bool SerializeJsonObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson)
	{
		OutJson.Reset();
		if (!Object.IsValid())
		{
			return false;
		}
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	}
}

FUeremcpIdempotencyStore::FUeremcpIdempotencyStore() = default;

FUeremcpIdempotencyStore& FUeremcpIdempotencyStore::Get()
{
	static FUeremcpIdempotencyStore Instance;
	return Instance;
}

FString FUeremcpIdempotencyStore::DefaultDurableRoot()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEREMCP"), TEXT("idempotency")));
}

void FUeremcpIdempotencyStore::SetDurableEnabled(bool bEnabled)
{
	bDurableEnabled = bEnabled;
}

void FUeremcpIdempotencyStore::SetDurableRootOverride(const FString& AbsoluteDirectory)
{
	DurableRootOverride = AbsoluteDirectory.IsEmpty()
		? FString()
		: FPaths::ConvertRelativePathToFull(AbsoluteDirectory);
}

FString FUeremcpIdempotencyStore::GetDurableRoot() const
{
	if (!DurableRootOverride.IsEmpty())
	{
		return DurableRootOverride;
	}
	return DefaultDurableRoot();
}

FString FUeremcpIdempotencyStore::DurableFileStemForKey(const FString& Key)
{
	FTCHARToUTF8 Utf8(*Key);
	uint8 Digest[UeremcpSha256::DigestBytes] = {};
	UeremcpSha256::Hash(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Digest);
	return UeremcpSha256::ToHex(Digest);
}

FString FUeremcpIdempotencyStore::DurableFilePathForKey(const FString& Key) const
{
	return FPaths::Combine(GetDurableRoot(), DurableFileStemForKey(Key) + TEXT(".json"));
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
	return LoadFromDurable(Key, OutResponseJson);
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
	if (bDurableEnabled)
	{
		WriteDurable(Key, ResponseJson);
	}
}

void FUeremcpIdempotencyStore::Clear()
{
	Entries.Empty();
}

void FUeremcpIdempotencyStore::PurgeDurable()
{
	Entries.Empty();
	if (!bDurableEnabled)
	{
		return;
	}

	const FString Root = GetDurableRoot();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Root, TEXT("*.json")), true, false);
	for (const FString& FileName : Files)
	{
		IFileManager::Get().Delete(*FPaths::Combine(Root, FileName), false, true);
	}
}

bool FUeremcpIdempotencyStore::LoadFromDurable(const FString& Key, FString& OutResponseJson) const
{
	if (!bDurableEnabled || Key.IsEmpty())
	{
		return false;
	}

	FString FileJson;
	const FString Path = DurableFilePathForKey(Key);
	if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(FileJson, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Record;
	if (!ParseJsonObject(FileJson, Record))
	{
		return false;
	}

	double Version = 0.0;
	if (!Record->TryGetNumberField(GRecordVersionField, Version)
		|| static_cast<int32>(Version) != GRecordVersion)
	{
		return false;
	}

	FString StoredKey;
	if (!Record->TryGetStringField(TEXT("idempotency_key"), StoredKey) || StoredKey != Key)
	{
		// Hash collision or tamper — refuse rather than replay the wrong response.
		return false;
	}

	const TSharedPtr<FJsonObject>* ResponseObject = nullptr;
	if (Record->TryGetObjectField(TEXT("response"), ResponseObject)
		&& ResponseObject
		&& ResponseObject->IsValid())
	{
		if (!SerializeJsonObject(*ResponseObject, OutResponseJson))
		{
			return false;
		}
	}
	else if (!Record->TryGetStringField(TEXT("response_json"), OutResponseJson)
		|| OutResponseJson.IsEmpty())
	{
		return false;
	}

	Entries.Add(Key, OutResponseJson);
	return true;
}

bool FUeremcpIdempotencyStore::WriteDurable(const FString& Key, const FString& ResponseJson) const
{
	const FString Root = GetDurableRoot();
	IFileManager::Get().MakeDirectory(*Root, true);

	TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
	Record->SetNumberField(GRecordVersionField, GRecordVersion);
	Record->SetStringField(TEXT("idempotency_key"), Key);
	Record->SetStringField(TEXT("stored_at_utc"), FDateTime::UtcNow().ToIso8601());

	TSharedPtr<FJsonObject> ResponseObject;
	if (ParseJsonObject(ResponseJson, ResponseObject))
	{
		Record->SetObjectField(TEXT("response"), ResponseObject);
	}
	else
	{
		Record->SetStringField(TEXT("response_json"), ResponseJson);
	}

	FString RecordJson;
	if (!SerializeJsonObject(Record, RecordJson))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(
		RecordJson,
		*DurableFilePathForKey(Key),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
