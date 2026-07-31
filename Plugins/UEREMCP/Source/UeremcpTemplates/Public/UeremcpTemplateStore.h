// UEREMCP — in-memory template store (ADR-0008). Owner: WS-15.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpTemplateTypes.h"

/**
 * Loads JSON templates from disk into an in-memory index keyed by template_id.
 * Canonical repo path: templates/ (see ADR-0008). Project overrides under
 * /Game/__UeremcpTemplates/ are a later phase.
 */
class UEREMCPTEMPLATES_API FUeremcpTemplateStore
{
public:
	void Reset();

	/** Load template documents and element presets recursively. Invalid files are skipped with OutErrors. */
	int32 LoadFromDirectory(const FString& RootDirectory, TArray<FString>& OutErrors);

	/**
	 * Like LoadFromDirectory but does not Reset(). New ids are inserted; duplicates are
	 * skipped with an error (agent quarantine must not clobber bundled fixtures).
	 */
	int32 MergeFromDirectory(const FString& RootDirectory, TArray<FString>& OutErrors);

	const FUeremcpTemplateRecord* FindById(const FString& TemplateId) const;
	const FUeremcpElementPreset* FindElementPreset(const FString& Element) const;

	void GetAllIds(TArray<FString>& OutIds) const;

	int32 Count() const { return Records.Num(); }
	int32 ElementPresetCount() const { return ElementPresets.Num(); }

	/**
	 * Insert or replace a template record in memory. Does not touch disk.
	 * Returns false if TemplateId is empty or Document is invalid.
	 */
	bool Upsert(const FUeremcpTemplateRecord& Record, FString& OutError);

	/**
	 * Serialize Record.Document to AbsoluteFilePath and Upsert into the index.
	 * Creates parent directories. Overwrites existing files.
	 */
	bool SaveDocument(
		const TSharedPtr<FJsonObject>& Document,
		const FString& AbsoluteFilePath,
		FUeremcpTemplateRecord& OutRecord,
		FString& OutError);

private:
	bool ParseTemplateFile(const FString& FilePath, FUeremcpTemplateRecord& OutRecord, FString& OutError) const;
	bool ParseElementPresetFile(const FString& FilePath, FUeremcpElementPreset& OutPreset, FString& OutError) const;
	static void IndexRecordMetadata(FUeremcpTemplateRecord& Record);

	TMap<FString, FUeremcpTemplateRecord> Records;
	TMap<FString, FUeremcpElementPreset> ElementPresets;
};
