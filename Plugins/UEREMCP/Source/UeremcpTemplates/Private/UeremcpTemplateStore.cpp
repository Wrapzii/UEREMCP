// UEREMCP — template store implementation. Owner: WS-15.

#include "UeremcpTemplateStore.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void FUeremcpTemplateStore::Reset()
{
	Records.Reset();
	ElementPresets.Reset();
}

int32 FUeremcpTemplateStore::LoadFromDirectory(const FString& RootDirectory, TArray<FString>& OutErrors)
{
	Reset();

	if (!FPaths::DirectoryExists(RootDirectory))
	{
		OutErrors.Add(FString::Printf(TEXT("template directory not found: %s"), *RootDirectory));
		return 0;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *RootDirectory, TEXT("*.json"), true, false);

	int32 Loaded = 0;
	for (const FString& FilePath : Files)
	{
		if (FPaths::GetCleanFilename(FPaths::GetPath(FilePath)).Equals(TEXT("elements"), ESearchCase::IgnoreCase))
		{
			FUeremcpElementPreset Preset;
			FString Error;
			if (!ParseElementPresetFile(FilePath, Preset, Error))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: %s"), *FilePath, *Error));
				continue;
			}
			const FString ElementKey = Preset.Element.ToLower();
			if (ElementPresets.Contains(ElementKey))
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: duplicate element preset '%s'"),
					*FilePath,
					*Preset.Element));
				continue;
			}
			ElementPresets.Add(ElementKey, MoveTemp(Preset));
			continue;
		}

		FUeremcpTemplateRecord Record;
		FString Error;
		if (!ParseTemplateFile(FilePath, Record, Error))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s"), *FilePath, *Error));
			continue;
		}

		if (Records.Contains(Record.TemplateId))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: duplicate template_id '%s'"),
				*FilePath,
				*Record.TemplateId));
			continue;
		}

		Records.Add(Record.TemplateId, MoveTemp(Record));
		++Loaded;
	}

	return Loaded;
}

const FUeremcpTemplateRecord* FUeremcpTemplateStore::FindById(const FString& TemplateId) const
{
	return Records.Find(TemplateId);
}

const FUeremcpElementPreset* FUeremcpTemplateStore::FindElementPreset(const FString& Element) const
{
	return ElementPresets.Find(Element.ToLower());
}

void FUeremcpTemplateStore::GetAllIds(TArray<FString>& OutIds) const
{
	Records.GetKeys(OutIds);
	OutIds.Sort();
}

bool FUeremcpTemplateStore::ParseTemplateFile(
	const FString& FilePath,
	FUeremcpTemplateRecord& OutRecord,
	FString& OutError) const
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		OutError = TEXT("failed to read file");
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("invalid JSON");
		return false;
	}

	if (!Root->TryGetStringField(TEXT("template_id"), OutRecord.TemplateId) || OutRecord.TemplateId.IsEmpty())
	{
		OutError = TEXT("missing template_id");
		return false;
	}

	Root->TryGetStringField(TEXT("domain"), OutRecord.Domain);
	Root->TryGetStringField(TEXT("category"), OutRecord.Category);
	Root->TryGetNumberField(TEXT("version"), OutRecord.Version);
	Root->TryGetStringField(TEXT("description"), OutRecord.Description);
	Root->TryGetStringField(TEXT("inherits_from"), OutRecord.InheritsFrom);

	const TArray<TSharedPtr<FJsonValue>>* Composes = nullptr;
	if (Root->TryGetArrayField(TEXT("composes"), Composes) && Composes)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Composes)
		{
			FString ComposedId;
			if (Value.IsValid() && Value->TryGetString(ComposedId))
			{
				OutRecord.Composes.Add(ComposedId);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Modifiers = nullptr;
	if (Root->TryGetArrayField(TEXT("supported_modifiers"), Modifiers) && Modifiers)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Modifiers)
		{
			FString ModifierName;
			if (Value.IsValid() && Value->TryGetString(ModifierName))
			{
				OutRecord.SupportedModifiers.Add(ModifierName);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* SearchTerms = nullptr;
	if (Root->TryGetArrayField(TEXT("search_terms"), SearchTerms) && SearchTerms)
	{
		for (const TSharedPtr<FJsonValue>& Value : *SearchTerms)
		{
			FString Term;
			if (Value.IsValid() && Value->TryGetString(Term))
			{
				OutRecord.SearchTerms.Add(Term);
			}
		}
	}

	OutRecord.Document = Root;
	OutRecord.SourcePath = FilePath;
	IndexRecordMetadata(OutRecord);
	return true;
}

bool FUeremcpTemplateStore::ParseElementPresetFile(
	const FString& FilePath,
	FUeremcpElementPreset& OutPreset,
	FString& OutError) const
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		OutError = TEXT("failed to read file");
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("invalid JSON");
		return false;
	}

	if (!Root->TryGetStringField(TEXT("preset_id"), OutPreset.PresetId) || OutPreset.PresetId.IsEmpty()
		|| !Root->TryGetStringField(TEXT("element"), OutPreset.Element) || OutPreset.Element.IsEmpty())
	{
		OutError = TEXT("missing preset_id or element");
		return false;
	}
	Root->TryGetNumberField(TEXT("version"), OutPreset.Version);

	const TSharedPtr<FJsonObject>* Material = nullptr;
	const TSharedPtr<FJsonObject>* Niagara = nullptr;
	const TSharedPtr<FJsonObject>* Parameters = nullptr;
	if (!Root->TryGetObjectField(TEXT("material"), Material) || !Material || !Material->IsValid()
		|| !(*Material)->TryGetObjectField(TEXT("parameter_overrides"), Parameters) || !Parameters || !Parameters->IsValid())
	{
		OutError = TEXT("missing material.parameter_overrides");
		return false;
	}
	OutPreset.MaterialParameterOverrides = *Parameters;

	Parameters = nullptr;
	if (!Root->TryGetObjectField(TEXT("niagara"), Niagara) || !Niagara || !Niagara->IsValid()
		|| !(*Niagara)->TryGetObjectField(TEXT("parameters"), Parameters) || !Parameters || !Parameters->IsValid())
	{
		OutError = TEXT("missing niagara.parameters");
		return false;
	}
	OutPreset.NiagaraParameters = *Parameters;
	OutPreset.SourcePath = FilePath;
	return true;
}

void FUeremcpTemplateStore::IndexRecordMetadata(FUeremcpTemplateRecord& Record)
{
	if (!Record.Document.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* Inputs = nullptr;
	if (Record.Document->TryGetObjectField(TEXT("inputs"), Inputs) && Inputs && Inputs->IsValid())
	{
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if ((*Inputs)->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
		{
			const TSharedPtr<FJsonObject>* Element = nullptr;
			if ((*Properties)->TryGetObjectField(TEXT("element"), Element) && Element && Element->IsValid())
			{
				Record.DeclaredElement = TEXT("parameterized");
			}
		}
	}
}
