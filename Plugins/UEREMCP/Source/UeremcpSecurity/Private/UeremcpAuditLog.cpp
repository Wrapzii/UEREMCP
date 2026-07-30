#include "UeremcpAuditLog.h"
#include "UeremcpPathPolicy.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UeremcpAuditLogPrivate
{
	static FCriticalSection& Mutex()
	{
		static FCriticalSection Instance;
		return Instance;
	}

	static TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
		return JsonValues;
	}

	static FString TierName(EUeremcpPermissionTier Tier)
	{
		switch (Tier)
		{
		case EUeremcpPermissionTier::Read:
			return TEXT("read");
		case EUeremcpPermissionTier::Write:
			return TEXT("write");
		case EUeremcpPermissionTier::Destructive:
			return TEXT("destructive");
		case EUeremcpPermissionTier::Unsafe:
			return TEXT("unsafe");
		default:
			return TEXT("unknown");
		}
	}

	static bool SerializeRecord(const FUeremcpAuditRecord& Record, FString& OutJson)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(
			TEXT("timestamp_utc"),
			Record.TimestampUtc.IsEmpty() ? FDateTime::UtcNow().ToIso8601() : Record.TimestampUtc);
		Object->SetStringField(TEXT("request_id"), Record.RequestId);
		Object->SetStringField(TEXT("idempotency_key"), Record.IdempotencyKey);
		Object->SetStringField(TEXT("action"), Record.Action);
		Object->SetStringField(TEXT("mode"), Record.Mode);
		Object->SetStringField(TEXT("status"), Record.Status);
		Object->SetStringField(TEXT("session_id"), Record.SessionId);
		Object->SetStringField(TEXT("target_asset_path"), Record.TargetAssetPath);
		Object->SetArrayField(TEXT("created_assets"), StringArray(Record.CreatedAssets));
		Object->SetArrayField(TEXT("modified_assets"), StringArray(Record.ModifiedAssets));
		Object->SetArrayField(TEXT("deleted_assets"), StringArray(Record.DeletedAssets));
		Object->SetBoolField(TEXT("dry_run"), Record.bDryRun);
		Object->SetBoolField(TEXT("atomic"), Record.bAtomic);
		Object->SetStringField(TEXT("required_tier"), TierName(Record.RequiredTier));
		Object->SetStringField(TEXT("revision_before"), Record.RevisionBefore);
		Object->SetStringField(TEXT("revision_after"), Record.RevisionAfter);
		Object->SetStringField(TEXT("project_path"), Record.ProjectPath);

		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		return FJsonSerializer::Serialize(Object, Writer);
	}
}

FString FUeremcpAuditLog::AuditDirectory(const FUeremcpPathPolicyRoots& Roots)
{
	return FPaths::Combine(FUeremcpPathPolicy::SavedUeremcpRoot(Roots), TEXT("audit"));
}

FString FUeremcpAuditLog::DailyLogFileName(const FDateTime& UtcNow)
{
	return FString::Printf(TEXT("%04d-%02d-%02d.jsonl"),
		UtcNow.GetYear(), UtcNow.GetMonth(), UtcNow.GetDay());
}

bool FUeremcpAuditLog::IsImplemented()
{
	return true;
}

bool FUeremcpAuditLog::Append(
	const FUeremcpAuditRecord& Record,
	const FUeremcpPathPolicyRoots& Roots,
	FString& OutError)
{
	OutError.Reset();
	if (!Roots.IsConfigured())
	{
		OutError = TEXT("audit roots are not configured");
		return false;
	}

	const FString Directory = AuditDirectory(Roots);
	const FString AuditFilePath = FPaths::Combine(Directory, DailyLogFileName());
	const FUeremcpPathValidationResult PathVerdict =
		FUeremcpPathPolicy::ValidateFilesystemPath(AuditFilePath, true, &Roots);
	if (!PathVerdict.bAllowed)
	{
		OutError = FString::Printf(TEXT("audit path rejected: %s"), *PathVerdict.Reason);
		return false;
	}

	FString Json;
	if (!UeremcpAuditLogPrivate::SerializeRecord(Record, Json))
	{
		OutError = TEXT("failed to serialize audit record");
		return false;
	}
	Json.AppendChar(TEXT('\n'));

	FScopeLock Lock(&UeremcpAuditLogPrivate::Mutex());
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*Directory) && !FileManager.MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("failed to create audit directory: %s"), *Directory);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		Json,
		*AuditFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&FileManager,
		FILEWRITE_Append))
	{
		OutError = FString::Printf(TEXT("failed to append audit record: %s"), *AuditFilePath);
		return false;
	}
	return true;
}

int32 FUeremcpAuditLog::PruneOlderThanDays(int32 RetentionDays, const FUeremcpPathPolicyRoots& Roots)
{
	if (RetentionDays < 1 || !Roots.IsConfigured())
	{
		return 0;
	}

	FScopeLock Lock(&UeremcpAuditLogPrivate::Mutex());
	IFileManager& FileManager = IFileManager::Get();
	const FString Directory = AuditDirectory(Roots);
	TArray<FString> FileNames;
	FileManager.FindFiles(FileNames, *Directory, TEXT("jsonl"));

	const FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(RetentionDays);
	int32 DeletedCount = 0;
	for (const FString& FileName : FileNames)
	{
		const FString FilePath = FPaths::Combine(Directory, FileName);
		if (FileManager.GetTimeStamp(*FilePath) < Cutoff
			&& FileManager.Delete(*FilePath, true, false, true))
		{
			++DeletedCount;
		}
	}
	return DeletedCount;
}
