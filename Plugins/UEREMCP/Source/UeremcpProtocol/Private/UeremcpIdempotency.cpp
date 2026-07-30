#include "UeremcpIdempotency.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UeremcpContentHash.h"
#include "UeremcpSha256.h"

namespace
{
	static constexpr const TCHAR* GRecordVersionField = TEXT("ueremcp_idempotency_record_version");
	static constexpr int32 GRecordVersion = 2;
	static constexpr int32 GLegacyRecordVersion = 1;
	static constexpr const TCHAR* GStateInProgress = TEXT("in_progress");
	static constexpr const TCHAR* GStateCompleted = TEXT("completed");

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

	static bool AppendCanonicalScalar(const TSharedPtr<FJsonValue>& Value, FString& Out)
	{
		FString Fragment;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Fragment);
		if (!Value.IsValid() || Value->IsNull())
		{
			Writer->WriteNull();
		}
		else if (Value->Type == EJson::Boolean)
		{
			Writer->WriteValue(Value->AsBool());
		}
		else if (Value->Type == EJson::Number)
		{
			Writer->WriteValue(Value->AsNumber());
		}
		else if (Value->Type == EJson::String)
		{
			Writer->WriteValue(Value->AsString());
		}
		else
		{
			return false;
		}
		Writer->Close();
		Out += Fragment;
		return true;
	}

	static bool AppendCanonicalJson(const TSharedPtr<FJsonValue>& Value, FString& Out)
	{
		if (!Value.IsValid() || Value->IsNull()
			|| Value->Type == EJson::Boolean
			|| Value->Type == EJson::Number
			|| Value->Type == EJson::String)
		{
			return AppendCanonicalScalar(Value, Out);
		}
		if (Value->Type == EJson::Array)
		{
			Out += TEXT("[");
			bool bFirst = true;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				if (!bFirst)
				{
					Out += TEXT(",");
				}
				bFirst = false;
				if (!AppendCanonicalJson(Item, Out))
				{
					return false;
				}
			}
			Out += TEXT("]");
			return true;
		}
		if (Value->Type != EJson::Object)
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		TArray<FString> Keys;
		for (const auto& Pair : Object->Values)
		{
			Keys.Add(FString(Pair.Key));
		}
		Keys.Sort([](const FString& A, const FString& B)
		{
			return A.Compare(B, ESearchCase::CaseSensitive) < 0;
		});
		Out += TEXT("{");
		bool bFirst = true;
		for (const FString& Key : Keys)
		{
			if (!bFirst)
			{
				Out += TEXT(",");
			}
			bFirst = false;
			if (!AppendCanonicalScalar(MakeShared<FJsonValueString>(Key), Out))
			{
				return false;
			}
			Out += TEXT(":");
			if (!AppendCanonicalJson(Object->TryGetField(Key), Out))
			{
				return false;
			}
		}
		Out += TEXT("}");
		return true;
	}

	static FString MutexName(const FString& Root, const FString& Key)
	{
		return TEXT("UEREMCP_Idempotency_")
			+ FUeremcpIdempotencyStore::DurableFileStemForKey(Root + TEXT("|") + Key).Left(32);
	}

	static bool IsMissingRecordError(const FString& Error)
	{
		return Error == TEXT("record_not_found");
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
	FScopeLock Lock(&EntriesMutex);
	bDurableEnabled = bEnabled;
}

void FUeremcpIdempotencyStore::SetDurableRootOverride(const FString& AbsoluteDirectory)
{
	FScopeLock Lock(&EntriesMutex);
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

bool FUeremcpIdempotencyStore::FingerprintRequestJson(
	const FString& RequestJson,
	FString& OutFingerprint,
	FString& OutError)
{
	OutFingerprint.Reset();
	OutError.Reset();
	TSharedPtr<FJsonObject> Request;
	if (!ParseJsonObject(RequestJson, Request))
	{
		OutError = TEXT("request is not a JSON object");
		return false;
	}
	Request->RemoveField(TEXT("request_id"));
	Request->RemoveField(TEXT("idempotency_key"));

	FString Canonical;
	if (!AppendCanonicalJson(MakeShared<FJsonValueObject>(Request), Canonical))
	{
		OutError = TEXT("failed to canonicalize request");
		return false;
	}
	OutFingerprint = FUeremcpContentHash::Sha256Prefixed(Canonical);
	return !OutFingerprint.IsEmpty();
}

FString FUeremcpIdempotencyStore::DurableFilePathForKey(const FString& Key) const
{
	return FPaths::Combine(GetDurableRoot(), DurableFileStemForKey(Key) + TEXT(".json"));
}

bool FUeremcpIdempotencyStore::IsExpired(const FRecord& Record, const FDateTime& Now) const
{
	return Record.State == GStateCompleted
		&& Retention > FTimespan::Zero()
		&& Record.UpdatedAtUtc != FDateTime::MinValue()
		&& Now - Record.UpdatedAtUtc > Retention;
}

bool FUeremcpIdempotencyStore::IsClaimStale(const FRecord& Record, const FDateTime& Now) const
{
	return Record.State == GStateInProgress
		&& StaleClaimAge > FTimespan::Zero()
		&& Record.UpdatedAtUtc != FDateTime::MinValue()
		&& Now - Record.UpdatedAtUtc > StaleClaimAge;
}

FUeremcpIdempotencyClaim FUeremcpIdempotencyStore::Claim(
	const FString& Key,
	const FString& RequestFingerprint,
	const FString& RequestId)
{
	FUeremcpIdempotencyClaim Result;
	if (Key.IsEmpty() || RequestFingerprint.IsEmpty())
	{
		Result.Error = TEXT("idempotency key and request fingerprint are required");
		return Result;
	}

	FScopeLock LocalLock(&EntriesMutex);
	TUniquePtr<FSystemWideCriticalSection> ProcessLock;
	if (bDurableEnabled)
	{
		ProcessLock = MakeUnique<FSystemWideCriticalSection>(
			MutexName(GetDurableRoot(), Key),
			FTimespan::FromSeconds(5));
		if (!ProcessLock->IsValid())
		{
			Result.Error = TEXT("timed out acquiring idempotency record lock");
			return Result;
		}
	}

	FRecord Existing;
	FString LoadError;
	bool bFound = false;
	if (bDurableEnabled)
	{
		bFound = LoadRecordFromDurable(Key, Existing, LoadError, true);
		if (!bFound && !LoadError.IsEmpty() && !IsMissingRecordError(LoadError))
		{
			Result.Error = LoadError;
			return Result;
		}
	}
	else if (const FRecord* Memory = Entries.Find(Key))
	{
		Existing = *Memory;
		bFound = true;
	}

	const FDateTime Now = FDateTime::UtcNow();
	if (bFound && IsExpired(Existing, Now))
	{
		FString DeleteError;
		if (bDurableEnabled && !DeleteRecordDurable(Key, DeleteError))
		{
			Result.Error = DeleteError;
			return Result;
		}
		Entries.Remove(Key);
		bFound = false;
	}

	if (bFound && Existing.RequestFingerprint != RequestFingerprint)
	{
		Result.Status = EUeremcpIdempotencyClaimStatus::Conflict;
		Result.Error = Existing.RequestFingerprint.IsEmpty()
			? TEXT("idempotency key belongs to a legacy unbound record; use a new key")
			: TEXT("idempotency key was already used for a different request");
		return Result;
	}

	if (bFound && Existing.State == GStateCompleted)
	{
		Result.Status = EUeremcpIdempotencyClaimStatus::Replay;
		Result.ResponseJson = AnnotateReplayedResponse(Existing.ResponseJson, RequestId);
		Entries.Add(Key, Existing);
		return Result;
	}

	if (bFound && Existing.State == GStateInProgress && !IsClaimStale(Existing, Now))
	{
		Result.Status = EUeremcpIdempotencyClaimStatus::InProgress;
		Result.Error = TEXT("matching idempotent request is already in progress");
		return Result;
	}

	FRecord ClaimRecord;
	ClaimRecord.Key = Key;
	ClaimRecord.RequestFingerprint = RequestFingerprint;
	ClaimRecord.State = GStateInProgress;
	ClaimRecord.CreatedAtUtc = bFound ? Existing.CreatedAtUtc : Now;
	ClaimRecord.UpdatedAtUtc = Now;

	FString WriteError;
	if (bDurableEnabled && !WriteRecordDurable(ClaimRecord, WriteError))
	{
		Result.Error = WriteError;
		return Result;
	}
	Entries.Add(Key, ClaimRecord);
	Result.Status = EUeremcpIdempotencyClaimStatus::Acquired;
	return Result;
}

bool FUeremcpIdempotencyStore::Complete(
	const FString& Key,
	const FString& RequestFingerprint,
	const FString& ResponseJson,
	FString& OutError)
{
	OutError.Reset();
	if (Key.IsEmpty() || RequestFingerprint.IsEmpty() || ResponseJson.IsEmpty())
	{
		OutError = TEXT("key, request fingerprint, and response are required");
		return false;
	}

	TSharedPtr<FJsonObject> Response;
	if (!ParseJsonObject(ResponseJson, Response))
	{
		OutError = TEXT("completed response is not a JSON object");
		return false;
	}

	FScopeLock LocalLock(&EntriesMutex);
	TUniquePtr<FSystemWideCriticalSection> ProcessLock;
	if (bDurableEnabled)
	{
		ProcessLock = MakeUnique<FSystemWideCriticalSection>(
			MutexName(GetDurableRoot(), Key),
			FTimespan::FromSeconds(5));
		if (!ProcessLock->IsValid())
		{
			OutError = TEXT("timed out acquiring idempotency record lock");
			return false;
		}
	}

	FRecord Existing;
	FString LoadError;
	bool bFound = bDurableEnabled
		? LoadRecordFromDurable(Key, Existing, LoadError, true)
		: Entries.RemoveAndCopyValue(Key, Existing);
	if (!bFound)
	{
		OutError = IsMissingRecordError(LoadError)
			? TEXT("idempotency claim is missing")
			: LoadError;
		return false;
	}
	if (Existing.RequestFingerprint != RequestFingerprint)
	{
		OutError = TEXT("idempotency claim fingerprint changed before completion");
		return false;
	}

	Existing.State = GStateCompleted;
	Existing.ResponseJson = ResponseJson;
	Existing.UpdatedAtUtc = FDateTime::UtcNow();
	if (bDurableEnabled && !WriteRecordDurable(Existing, OutError))
	{
		return false;
	}
	Entries.Add(Key, Existing);
	return true;
}

bool FUeremcpIdempotencyStore::Abandon(
	const FString& Key,
	const FString& RequestFingerprint,
	FString& OutError)
{
	OutError.Reset();
	FScopeLock LocalLock(&EntriesMutex);
	TUniquePtr<FSystemWideCriticalSection> ProcessLock;
	if (bDurableEnabled)
	{
		ProcessLock = MakeUnique<FSystemWideCriticalSection>(
			MutexName(GetDurableRoot(), Key),
			FTimespan::FromSeconds(5));
		if (!ProcessLock->IsValid())
		{
			OutError = TEXT("timed out acquiring idempotency record lock");
			return false;
		}
	}

	FRecord Existing;
	FString LoadError;
	const bool bFound = bDurableEnabled
		? LoadRecordFromDurable(Key, Existing, LoadError, true)
		: Entries.RemoveAndCopyValue(Key, Existing);
	if (!bFound)
	{
		return IsMissingRecordError(LoadError);
	}
	if (Existing.State != GStateInProgress || Existing.RequestFingerprint != RequestFingerprint)
	{
		OutError = TEXT("only the matching in-progress claim can be abandoned");
		return false;
	}
	Entries.Remove(Key);
	return !bDurableEnabled || DeleteRecordDurable(Key, OutError);
}

bool FUeremcpIdempotencyStore::Remove(const FString& Key, FString& OutError)
{
	OutError.Reset();
	FScopeLock LocalLock(&EntriesMutex);
	TUniquePtr<FSystemWideCriticalSection> ProcessLock;
	if (bDurableEnabled)
	{
		ProcessLock = MakeUnique<FSystemWideCriticalSection>(
			MutexName(GetDurableRoot(), Key),
			FTimespan::FromSeconds(5));
		if (!ProcessLock->IsValid())
		{
			OutError = TEXT("timed out acquiring idempotency record lock");
			return false;
		}
	}
	Entries.Remove(Key);
	return !bDurableEnabled || DeleteRecordDurable(Key, OutError);
}

bool FUeremcpIdempotencyStore::TryGet(const FString& Key, FString& OutResponseJson) const
{
	if (Key.IsEmpty())
	{
		return false;
	}
	FScopeLock Lock(&EntriesMutex);
	if (const FRecord* Found = Entries.Find(Key))
	{
		if (Found->State == GStateCompleted)
		{
			OutResponseJson = Found->ResponseJson;
			return true;
		}
		return false;
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
	FScopeLock Lock(&EntriesMutex);
	const FDateTime Now = FDateTime::UtcNow();
	FRecord Record;
	Record.Key = Key;
	Record.State = GStateCompleted;
	Record.ResponseJson = ResponseJson;
	Record.CreatedAtUtc = Now;
	Record.UpdatedAtUtc = Now;
	Entries.Add(Key, Record);
	if (bDurableEnabled)
	{
		FString IgnoredError;
		WriteRecordDurable(Record, IgnoredError);
	}
}

void FUeremcpIdempotencyStore::Clear()
{
	FScopeLock Lock(&EntriesMutex);
	Entries.Empty();
}

void FUeremcpIdempotencyStore::PurgeDurable()
{
	FScopeLock Lock(&EntriesMutex);
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
	FRecord Record;
	FString Error;
	if (!LoadRecordFromDurable(Key, Record, Error, true)
		|| Record.State != GStateCompleted
		|| IsExpired(Record, FDateTime::UtcNow()))
	{
		return false;
	}
	OutResponseJson = Record.ResponseJson;
	Entries.Add(Key, Record);
	return true;
}

bool FUeremcpIdempotencyStore::WriteDurable(const FString& Key, const FString& ResponseJson) const
{
	const FDateTime Now = FDateTime::UtcNow();
	FRecord Record;
	Record.Key = Key;
	Record.State = GStateCompleted;
	Record.ResponseJson = ResponseJson;
	Record.CreatedAtUtc = Now;
	Record.UpdatedAtUtc = Now;
	FString Error;
	return WriteRecordDurable(Record, Error);
}

bool FUeremcpIdempotencyStore::LoadRecordFromDurable(
	const FString& Key,
	FRecord& OutRecord,
	FString& OutError,
	bool bQuarantineCorrupt) const
{
	OutError.Reset();
	const FString Path = DurableFilePathForKey(Key);
	if (!FPaths::FileExists(Path))
	{
		OutError = TEXT("record_not_found");
		return false;
	}

	FString FileJson;
	TSharedPtr<FJsonObject> RecordObject;
	if (!FFileHelper::LoadFileToString(FileJson, *Path) || !ParseJsonObject(FileJson, RecordObject))
	{
		OutError = TEXT("idempotency record is unreadable or corrupt");
		if (bQuarantineCorrupt)
		{
			const FString Quarantine = Path + TEXT(".corrupt.")
				+ FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"));
			IFileManager::Get().Move(*Quarantine, *Path, false, false, false, true);
		}
		return false;
	}

	double VersionNumber = 0.0;
	if (!RecordObject->TryGetNumberField(GRecordVersionField, VersionNumber))
	{
		OutError = TEXT("idempotency record has no schema version");
		return false;
	}
	const int32 Version = static_cast<int32>(VersionNumber);
	if (Version != GRecordVersion && Version != GLegacyRecordVersion)
	{
		OutError = TEXT("unsupported idempotency record schema version");
		return false;
	}

	if (!RecordObject->TryGetStringField(TEXT("idempotency_key"), OutRecord.Key)
		|| OutRecord.Key != Key)
	{
		OutError = TEXT("idempotency record key mismatch");
		return false;
	}

	OutRecord.State = GStateCompleted;
	RecordObject->TryGetStringField(TEXT("request_fingerprint"), OutRecord.RequestFingerprint);
	RecordObject->TryGetStringField(TEXT("state"), OutRecord.State);
	if (OutRecord.State != GStateInProgress && OutRecord.State != GStateCompleted)
	{
		OutError = TEXT("idempotency record has invalid state");
		return false;
	}

	FString CreatedAt;
	FString UpdatedAt;
	RecordObject->TryGetStringField(TEXT("created_at_utc"), CreatedAt);
	if (!RecordObject->TryGetStringField(TEXT("updated_at_utc"), UpdatedAt))
	{
		RecordObject->TryGetStringField(TEXT("stored_at_utc"), UpdatedAt);
	}
	if (!CreatedAt.IsEmpty())
	{
		FDateTime::ParseIso8601(*CreatedAt, OutRecord.CreatedAtUtc);
	}
	if (!UpdatedAt.IsEmpty())
	{
		FDateTime::ParseIso8601(*UpdatedAt, OutRecord.UpdatedAtUtc);
	}
	if (OutRecord.CreatedAtUtc == FDateTime::MinValue())
	{
		OutRecord.CreatedAtUtc = OutRecord.UpdatedAtUtc;
	}

	if (OutRecord.State == GStateCompleted)
	{
		const TSharedPtr<FJsonObject>* ResponseObject = nullptr;
		if (RecordObject->TryGetStringField(TEXT("response_json"), OutRecord.ResponseJson)
			&& !OutRecord.ResponseJson.IsEmpty())
		{
			// Prefer the exact stored bytes so Put/Complete round-trips are bit-stable.
		}
		else if (RecordObject->TryGetObjectField(TEXT("response"), ResponseObject)
			&& ResponseObject
			&& ResponseObject->IsValid())
		{
			if (!SerializeJsonObject(*ResponseObject, OutRecord.ResponseJson))
			{
				OutError = TEXT("idempotency response cannot be serialized");
				return false;
			}
		}
		else
		{
			OutError = TEXT("completed idempotency record has no response");
			return false;
		}
	}
	return true;
}

bool FUeremcpIdempotencyStore::WriteRecordDurable(
	const FRecord& Record,
	FString& OutError) const
{
	OutError.Reset();
	const FString Root = GetDurableRoot();
	if (!IFileManager::Get().MakeDirectory(*Root, true)
		&& !IFileManager::Get().DirectoryExists(*Root))
	{
		OutError = TEXT("failed to create idempotency directory");
		return false;
	}

	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(GRecordVersionField, GRecordVersion);
	Object->SetStringField(TEXT("idempotency_key"), Record.Key);
	Object->SetStringField(TEXT("request_fingerprint"), Record.RequestFingerprint);
	Object->SetStringField(TEXT("state"), Record.State);
	Object->SetStringField(TEXT("created_at_utc"), Record.CreatedAtUtc.ToIso8601());
	Object->SetStringField(TEXT("updated_at_utc"), Record.UpdatedAtUtc.ToIso8601());
	if (Record.State == GStateCompleted)
	{
		TSharedPtr<FJsonObject> Response;
		if (ParseJsonObject(Record.ResponseJson, Response))
		{
			Object->SetObjectField(TEXT("response"), Response);
			Object->SetStringField(TEXT("response_json"), Record.ResponseJson);
		}
		else
		{
			OutError = TEXT("refusing to persist a non-object response");
			return false;
		}
	}

	FString Json;
	if (!SerializeJsonObject(Object, Json))
	{
		OutError = TEXT("failed to serialize idempotency record");
		return false;
	}

	const FString Destination = DurableFilePathForKey(Record.Key);
	const FString Temp = Destination + TEXT(".tmp.")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!FFileHelper::SaveStringToFile(
			Json,
			*Temp,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = TEXT("failed to write idempotency temp record");
		return false;
	}
	if (!IFileManager::Get().Move(*Destination, *Temp, true, false, false, true))
	{
		IFileManager::Get().Delete(*Temp, false, true);
		OutError = TEXT("failed to atomically replace idempotency record");
		return false;
	}
	return true;
}

bool FUeremcpIdempotencyStore::DeleteRecordDurable(
	const FString& Key,
	FString& OutError) const
{
	OutError.Reset();
	const FString Path = DurableFilePathForKey(Key);
	if (!FPaths::FileExists(Path))
	{
		return true;
	}
	if (!IFileManager::Get().Delete(*Path, false, true))
	{
		OutError = TEXT("failed to delete idempotency record");
		return false;
	}
	return true;
}
