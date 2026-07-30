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

	/** Load every *.json file under RootDirectory recursively. Invalid files are skipped with OutErrors. */
	int32 LoadFromDirectory(const FString& RootDirectory, TArray<FString>& OutErrors);

	const FUeremcpTemplateRecord* FindById(const FString& TemplateId) const;

	void GetAllIds(TArray<FString>& OutIds) const;

	int32 Count() const { return Records.Num(); }

private:
	bool ParseTemplateFile(const FString& FilePath, FUeremcpTemplateRecord& OutRecord, FString& OutError) const;
	static void IndexRecordMetadata(FUeremcpTemplateRecord& Record);

	TMap<FString, FUeremcpTemplateRecord> Records;
};
