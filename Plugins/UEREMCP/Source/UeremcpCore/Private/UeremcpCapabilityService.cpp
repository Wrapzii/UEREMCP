#include "UeremcpCapabilityService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "UObject/SoftObjectPath.h"
#include "UeremcpContentHash.h"
#include "UeremcpEnvelope.h"
#include "UeremcpIdempotency.h"
#include "UeremcpSchemaPublishing.h"

namespace UeremcpCapabilityInternal
{
	struct FToolRecord
	{
		FString Toolset;
		FString Tool;
		FString Qualified;
		FString Description;
		FString Domain;
		FString Risk;
		FString Lifecycle;
		TSharedPtr<FJsonObject> InputSchema;
		FString ContractHash;
		TArray<FString> Required;
		TSet<FString> SpecificationFields;
		TArray<FString> AssetTypes;
		bool bMetadataExplicit = false;
	};

	struct FRegistryMetadata
	{
		FString Domain;
		FString Risk;
		FString Lifecycle;
		TArray<FString> AssetTypes;
		bool bExplicit = false;
	};

	struct FPreparedAction
	{
		FString ActionId;
		FString ContextId;
		FString Goal;
		FString Tool;
		FString Domain;
		FString Risk;
		FString RiskCeiling;
		FString Lifecycle;
		FString RegistryHash;
		FString ContractHash;
		FString ExpectedRevision;
		FString AssetPath;
		FString ExpiresAt;
		FString ResourceId;
		FString ContinuationKind;
		FString RecommendationReason;
		double Confidence = 0.0;
		FDateTime ExpiresAtUtc;
		TSharedPtr<FJsonObject> Arguments;
		TArray<FString> ScopeAssetPaths;
		TSet<FString> SpecificationFields;
		TArray<FString> MissingArguments;
		bool bConfirmationRequired = false;
		bool bResourceExisted = false;
	};

	FCriticalSection& StoreMutex()
	{
		static FCriticalSection Mutex;
		return Mutex;
	}

	TMap<FString, FPreparedAction>& Actions()
	{
		static TMap<FString, FPreparedAction> Store;
		return Store;
	}

	FString JsonObjectToString(const TSharedPtr<FJsonObject>& Object)
	{
		FString Out;
		if (!Object.IsValid())
		{
			return Out;
		}
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Out;
	}

	bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& Out, FString& Error)
	{
		Out.Reset();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			Error = TEXT("request must be a JSON object");
			return false;
		}
		return true;
	}

	TSharedPtr<FJsonObject> SpecFor(const TSharedPtr<FJsonObject>& Root)
	{
		const TSharedPtr<FJsonObject>* Spec = nullptr;
		if (Root.IsValid() && Root->TryGetObjectField(TEXT("specification"), Spec) && Spec)
		{
			return *Spec;
		}
		return Root;
	}

	FString StringField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Default = FString())
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(Field, Value) ? Value : Default;
	}

	int32 IntField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Default)
	{
		if (Obj.IsValid() && Obj->HasField(Field))
		{
			return FMath::Clamp(static_cast<int32>(Obj->GetNumberField(Field)), 1, 32);
		}
		return Default;
	}

	FString Lower(const FString& Value)
	{
		return Value.ToLower();
	}

	int32 RiskRank(const FString& Risk)
	{
		if (Risk == TEXT("read_only")) return 0;
		if (Risk == TEXT("creates_asset")) return 1;
		if (Risk == TEXT("modifies_asset")) return 2;
		if (Risk == TEXT("saves_asset")) return 3;
		return 4;
	}

	bool RiskWithin(const FString& Risk, const FString& Ceiling)
	{
		const FString EffectiveCeiling = Ceiling.IsEmpty() ? TEXT("read_only") : Ceiling;
		const bool bKnownRisk = Risk == TEXT("read_only") || Risk == TEXT("creates_asset")
			|| Risk == TEXT("modifies_asset") || Risk == TEXT("saves_asset") || Risk == TEXT("destructive");
		const bool bKnownCeiling = EffectiveCeiling == TEXT("read_only") || EffectiveCeiling == TEXT("creates_asset")
			|| EffectiveCeiling == TEXT("modifies_asset") || EffectiveCeiling == TEXT("saves_asset") || EffectiveCeiling == TEXT("destructive");
		return bKnownRisk && bKnownCeiling && RiskRank(Risk) <= RiskRank(EffectiveCeiling);
	}

	bool IsAllowedOption(const FString& Key)
	{
		return Key == TEXT("dry_run") || Key == TEXT("response_detail") || Key == TEXT("validate")
			|| Key == TEXT("compile") || Key == TEXT("save") || Key == TEXT("timeout_ms");
	}

	FRegistryMetadata ExplicitMetadata(const FString& Qualified, const FString& Toolset, const FString& Tool)
	{
		FRegistryMetadata Metadata;
		Metadata.Domain = TEXT("general");
		Metadata.Risk = TEXT("read_only");
		Metadata.Lifecycle = TEXT("operate");

		// This is server-owned registry metadata. It is deliberately independent of
		// free-form tool descriptions, which are not a safety authority.
		static const TMap<FString, FRegistryMetadata> Exact = {
			{TEXT("UeremcpCore.UeremcpReferenceToolset.GetStarted"), {TEXT("general"), TEXT("read_only"), TEXT("inspect"), {}}},
			{TEXT("UeremcpCore.UeremcpReferenceToolset.ResolveIntent"), {TEXT("general"), TEXT("read_only"), TEXT("discover"), {}}},
			{TEXT("UeremcpCore.UeremcpReferenceToolset.DescribeOperation"), {TEXT("general"), TEXT("read_only"), TEXT("inspect"), {}}},
			{TEXT("UeremcpCore.UeremcpReferenceToolset.SearchCapabilities"), {TEXT("general"), TEXT("read_only"), TEXT("discover"), {}}},
			{TEXT("UeremcpCore.UeremcpReferenceToolset.ResolveAndPrepare"), {TEXT("general"), TEXT("read_only"), TEXT("discover"), {}}},
			{TEXT("UeremcpCore.UeremcpReferenceToolset.GetCapabilityContract"), {TEXT("general"), TEXT("read_only"), TEXT("inspect"), {}}},
			{TEXT("UeremcpCore.UeremcpReferenceToolset.ExecutePreparedAction"), {TEXT("general"), TEXT("read_only"), TEXT("operate"), {}}},
			{TEXT("UeremcpNiagara.UeremcpNiagaraToolset.InspectSystem"), {TEXT("niagara"), TEXT("read_only"), TEXT("inspect"), {TEXT("NiagaraSystem")}}},
			{TEXT("UeremcpNiagara.UeremcpNiagaraToolset.CreateNiagaraEffect"), {TEXT("niagara"), TEXT("creates_asset"), TEXT("create"), {TEXT("NiagaraSystem")}}},
			{TEXT("UeremcpNiagara.UeremcpNiagaraToolset.AdaptNiagaraEffect"), {TEXT("niagara"), TEXT("modifies_asset"), TEXT("modify"), {TEXT("NiagaraSystem")}}},
			{TEXT("UeremcpNiagara.UeremcpNiagaraToolset.SubmitNiagaraGraph"), {TEXT("niagara"), TEXT("modifies_asset"), TEXT("modify"), {TEXT("NiagaraSystem")}}},
			{TEXT("UeremcpMaterial.UeremcpMaterialToolset.InspectMaterial"), {TEXT("material"), TEXT("read_only"), TEXT("inspect"), {TEXT("Material"), TEXT("MaterialInstance")}}},
			{TEXT("UeremcpMaterial.UeremcpMaterialToolset.CreateMasterMaterial"), {TEXT("material"), TEXT("creates_asset"), TEXT("create"), {TEXT("Material")}}},
			{TEXT("UeremcpMaterial.UeremcpMaterialToolset.CreateLandscapeMaterial"), {TEXT("material"), TEXT("creates_asset"), TEXT("create"), {TEXT("Material")}}},
			{TEXT("UeremcpMaterial.UeremcpMaterialToolset.SubmitMaterialGraph"), {TEXT("material"), TEXT("modifies_asset"), TEXT("modify"), {TEXT("Material"), TEXT("MaterialInstance")}}},
			{TEXT("UeremcpBlueprint.UeremcpBlueprintToolset.ReadGraph"), {TEXT("blueprint"), TEXT("read_only"), TEXT("inspect"), {TEXT("Blueprint")}}},
			{TEXT("UeremcpBlueprint.UeremcpBlueprintToolset.SubmitGraph"), {TEXT("blueprint"), TEXT("modifies_asset"), TEXT("modify"), {TEXT("Blueprint")}}},
			{TEXT("editor_toolset.toolsets.data_asset.DataAssetTools.create"), {TEXT("asset"), TEXT("creates_asset"), TEXT("create"), {TEXT("DataAsset")}}},
			{TEXT("editor_toolset.toolsets.asset.AssetTools.get_asset_class"), {TEXT("asset"), TEXT("read_only"), TEXT("inspect"), {TEXT("UObject"), TEXT("DataAsset")}}},
			{TEXT("UeremcpValidation.UeremcpVisualCaptureToolset.CaptureEffectFrames"), {TEXT("validation"), TEXT("read_only"), TEXT("preview"), {TEXT("NiagaraSystem")}}},
			{TEXT("UeremcpValidation.UeremcpVisualCaptureToolset.CaptureMaterialFrames"), {TEXT("validation"), TEXT("read_only"), TEXT("preview"), {TEXT("Material"), TEXT("MaterialInstance")}}},
		};
		if (const FRegistryMetadata* Found = Exact.Find(Qualified))
		{
			Metadata = *Found;
			Metadata.bExplicit = true;
			return Metadata;
		}

		// Explicit toolset registrations cover primitive read/inspection tools. Any
		// unknown mutator remains unavailable to preparation until it receives an
		// explicit entry above.
		static const TArray<TPair<FString, FString>> Domains = {
			{TEXT("NiagaraToolsets."), TEXT("niagara")},
			{TEXT("UeremcpNiagara."), TEXT("niagara")},
			{TEXT("editor_toolset.toolsets.material"), TEXT("material")},
			{TEXT("UeremcpMaterial."), TEXT("material")},
			{TEXT("editor_toolset.toolsets.blueprint"), TEXT("blueprint")},
			{TEXT("UeremcpBlueprint."), TEXT("blueprint")},
			{TEXT("UeremcpValidation."), TEXT("validation")},
			{TEXT("UeremcpEnvironment."), TEXT("environment")},
			{TEXT("UeremcpGameplay."), TEXT("gameplay")},
			{TEXT("UeremcpAnimation."), TEXT("animation")},
			{TEXT("UeremcpUI."), TEXT("ui")},
		};
		for (const TPair<FString, FString>& Pair : Domains)
		{
			if (Toolset.StartsWith(Pair.Key))
			{
				Metadata.Domain = Pair.Value;
				Metadata.bExplicit = true;
				break;
			}
		}
		return Metadata;
	}

	FString CamelToSnake(const FString& Value)
	{
		FString Out;
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR C = Value[Index];
			if (FChar::IsUpper(C) && Index > 0)
			{
				Out.AppendChar(TEXT('_'));
			}
			Out.AppendChar(FChar::ToLower(C));
		}
		return Out;
	}

	FString AssetTypeForPath(const FString& Path)
	{
		const FString N = Lower(Path);
		if (N.Contains(TEXT("niagara")) || N.Contains(TEXT("/ns_")) || N.Contains(TEXT(".ns_"))) return TEXT("NiagaraSystem");
		if (N.Contains(TEXT("material")) || N.Contains(TEXT("/m_")) || N.Contains(TEXT("/mi_"))) return TEXT("Material");
		if (N.Contains(TEXT("blueprint")) || N.Contains(TEXT("/bp_"))) return TEXT("Blueprint");
		if (N.Contains(TEXT("/da_")) || N.Contains(TEXT(".da_"))) return TEXT("DataAsset");
		return TEXT("UObject");
	}

	FString AssetTypeForData(const FAssetData& Asset)
	{
		const FString ClassName = Asset.AssetClassPath.ToString();
		if (ClassName.Contains(TEXT("NiagaraSystem"))) return TEXT("NiagaraSystem");
		if (ClassName.Contains(TEXT("MaterialInstance"))) return TEXT("MaterialInstance");
		if (ClassName.EndsWith(TEXT("Material"))) return TEXT("Material");
		if (ClassName.Contains(TEXT("Blueprint"))) return TEXT("Blueprint");
		if (ClassName.Contains(TEXT("DataAsset"))) return TEXT("DataAsset");
		return AssetTypeForPath(Asset.GetObjectPathString());
	}

	bool FindAssetData(const FString& AssetPath, FAssetData& OutAsset)
	{
		if (AssetPath.IsEmpty()) return false;
		FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = Module.Get();
		const FString Name = FPackageName::GetLongPackageAssetName(AssetPath);
		const FSoftObjectPath ObjectPath(FString::Printf(TEXT("%s.%s"), *AssetPath, *Name));
		OutAsset = Registry.GetAssetByObjectPath(ObjectPath);
		return OutAsset.IsValid();
	}

	FString AssetClassForPath(const FString& AssetPath)
	{
		FAssetData Asset;
		return FindAssetData(AssetPath, Asset) ? Asset.AssetClassPath.ToString() : FString();
	}

	bool SearchAssets(const FString& Query, const FString& SearchRoot, const FString& RequestedType, TArray<FString>& OutPaths)
	{
		OutPaths.Reset();
		if (Query.IsEmpty()) return false;
		FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = Module.Get();
		if (Registry.IsLoadingAssets()) return false;
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(FName(*(SearchRoot.IsEmpty() ? TEXT("/Game") : SearchRoot)));
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		const FString Needle = Query.ToLower();
		const FString TypeNeedle = RequestedType.ToLower();
		for (const FAssetData& Asset : Assets)
		{
			const FString AssetName = Asset.AssetName.ToString();
			const FString ObjectPath = Asset.GetObjectPathString();
			if (!AssetName.ToLower().Contains(Needle) && !ObjectPath.ToLower().Contains(Needle)) continue;
			if (!TypeNeedle.IsEmpty() && !AssetTypeForData(Asset).ToLower().Contains(TypeNeedle)) continue;
			OutPaths.Add(Asset.PackageName.ToString());
			if (OutPaths.Num() >= 8) break;
		}
		return OutPaths.Num() > 0;
	}

	FString RegistryHash(const TArray<FToolRecord>& Records)
	{
		TArray<FString> Rows;
		for (const FToolRecord& Record : Records)
		{
			Rows.Add(Record.Qualified + TEXT("|") + Record.ContractHash + TEXT("|") + Record.Risk + TEXT("|") + Record.Domain);
		}
		Rows.Sort();
		return FUeremcpContentHash::Sha256Prefixed(FString::Join(Rows, TEXT("\n")));
	}

	TSharedPtr<FJsonObject> CompactContract(const FToolRecord& Record)
	{
		TSharedPtr<FJsonObject> Contract = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Required;
		for (const FString& Field : Record.Required)
		{
			Required.Add(MakeShared<FJsonValueString>(Field));
			const TSharedPtr<FJsonObject>* Properties = nullptr;
			if (Record.InputSchema.IsValid() && Record.InputSchema->TryGetObjectField(TEXT("properties"), Properties) && Properties)
			{
				const TSharedPtr<FJsonObject>* FieldSchema = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* NestedRequired = nullptr;
				if ((*Properties)->TryGetObjectField(Field, FieldSchema) && FieldSchema
					&& (*FieldSchema)->TryGetArrayField(TEXT("required"), NestedRequired) && NestedRequired)
				{
					for (const TSharedPtr<FJsonValue>& Nested : *NestedRequired)
						Required.Add(MakeShared<FJsonValueString>(Field + TEXT(".") + Nested->AsString()));
				}
			}
		}
		Contract->SetArrayField(TEXT("required_arguments"), Required);
		TArray<TSharedPtr<FJsonValue>> Editable;
		for (const FString& Field : Record.SpecificationFields) Editable.Add(MakeShared<FJsonValueString>(Field));
		Contract->SetArrayField(TEXT("editable_fields"), Editable);
		Contract->SetStringField(TEXT("target_identity_rules"), TEXT("Use target.asset_path; prepared execution rejects paths outside the original scope."));
		TArray<TSharedPtr<FJsonValue>> OutputKeys;
		for (const TCHAR* Key : {TEXT("status"), TEXT("result"), TEXT("revision"), TEXT("validation")}) OutputKeys.Add(MakeShared<FJsonValueString>(Key));
		Contract->SetArrayField(TEXT("output_keys"), OutputKeys);
		TArray<TSharedPtr<FJsonValue>> FailureClasses;
		for (const TCHAR* Key : {TEXT("expired"), TEXT("registry_mismatch"), TEXT("contract_mismatch"), TEXT("stale_revision"), TEXT("permission_denied"), TEXT("invalid_override")}) FailureClasses.Add(MakeShared<FJsonValueString>(Key));
		Contract->SetArrayField(TEXT("failure_classes"), FailureClasses);
		TArray<TSharedPtr<FJsonValue>> AllowedEnums;
		Contract->SetArrayField(TEXT("allowed_enums"), AllowedEnums);
		Contract->SetStringField(TEXT("side_effects"), Record.Risk == TEXT("read_only") ? TEXT("none declared") : Record.Risk);
		Contract->SetStringField(TEXT("minimal_invocation"), FString::Printf(TEXT("{\"protocol_version\":\"1.0\",\"action\":\"%s\",\"specification\":{}}"), *CamelToSnake(Record.Tool)));
		Contract->SetStringField(TEXT("input_schema_hash"), Record.ContractHash);
		Contract->SetStringField(TEXT("output_schema_hash"), FUeremcpContentHash::Sha256Prefixed(TEXT("ueremcp.response.v1")));
		return Contract;
	}

	bool ParseLiveRecords(TArray<FToolRecord>& OutRecords, FString& Error)
	{
		OutRecords.Reset();
		if (!UToolsetRegistry::IsAvailable())
		{
			Error = TEXT("ToolsetRegistry unavailable");
			return false;
		}
		TSharedPtr<FJsonValue> Root;
		const FString Raw = UToolsetRegistry::GetAllToolsetJsonSchemas();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Error = TEXT("live ToolsetRegistry schema payload is invalid JSON");
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Toolsets;
		if (Root->Type == EJson::Array)
		{
			Toolsets = Root->AsArray();
		}
		else if (Root->Type == EJson::Object)
		{
			const TArray<TSharedPtr<FJsonValue>>* Nested = nullptr;
			if (Root->AsObject()->TryGetArrayField(TEXT("toolsets"), Nested) && Nested) Toolsets = *Nested;
		}
		for (const TSharedPtr<FJsonValue>& ToolsetValue : Toolsets)
		{
			if (!ToolsetValue.IsValid() || ToolsetValue->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> Toolset = ToolsetValue->AsObject();
			const FString ToolsetName = StringField(Toolset, TEXT("name"));
			const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
			if (ToolsetName.IsEmpty() || !Toolset->TryGetArrayField(TEXT("tools"), Tools) || !Tools) continue;
			for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
			{
				if (!ToolValue.IsValid() || ToolValue->Type != EJson::Object) continue;
				const TSharedPtr<FJsonObject> ToolObj = ToolValue->AsObject();
				FString Name = StringField(ToolObj, TEXT("name"));
				if (Name.IsEmpty()) continue;
				FString ShortName = Name;
				int32 Dot = INDEX_NONE;
				if (Name.FindLastChar(TEXT('.'), Dot)) ShortName = Name.RightChop(Dot + 1);
				FToolRecord Record;
				Record.Toolset = ToolsetName;
				Record.Tool = ShortName;
				Record.Qualified = ToolsetName + TEXT(".") + ShortName;
				Record.Description = StringField(ToolObj, TEXT("description"));
				const FRegistryMetadata Metadata = ExplicitMetadata(Record.Qualified, ToolsetName, ShortName);
				Record.Domain = Metadata.Domain;
				Record.Risk = Metadata.Risk;
				Record.Lifecycle = Metadata.Lifecycle;
				Record.AssetTypes = Metadata.AssetTypes;
				Record.bMetadataExplicit = Metadata.bExplicit;
				const TSharedPtr<FJsonObject>* Schema = nullptr;
				if (ToolObj->TryGetObjectField(TEXT("inputSchema"), Schema) && Schema)
				{
					Record.InputSchema = *Schema;
					const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
					if (Record.InputSchema->TryGetArrayField(TEXT("required"), Required) && Required)
						for (const TSharedPtr<FJsonValue>& Value : *Required) Record.Required.Add(Value->AsString());
					const TSharedPtr<FJsonObject>* Properties = nullptr;
					if (Record.InputSchema->TryGetObjectField(TEXT("properties"), Properties) && Properties)
					{
						const TSharedPtr<FJsonObject>* Specification = nullptr;
						if ((*Properties)->TryGetObjectField(TEXT("specification"), Specification) && Specification)
						{
							const TSharedPtr<FJsonObject>* SpecProps = nullptr;
							if ((*Specification)->TryGetObjectField(TEXT("properties"), SpecProps) && SpecProps)
								for (const auto& Pair : (*SpecProps)->Values) Record.SpecificationFields.Add(FString(Pair.Key));
						}
					}
				}
				TSharedPtr<FJsonObject> Contract = CompactContract(Record);
				Record.ContractHash = FUeremcpContentHash::HashJsonObject(Contract);
				OutRecords.Add(MoveTemp(Record));
			}
		}
		if (OutRecords.Num() == 0) Error = TEXT("live ToolsetRegistry returned no tools");
		return OutRecords.Num() > 0;
	}

	FString CurrentRegistryHash(TArray<FToolRecord>* OutRecords = nullptr)
	{
		TArray<FToolRecord> Records;
		FString Error;
		if (!ParseLiveRecords(Records, Error)) return FString();
		const FString Hash = RegistryHash(Records);
		if (OutRecords) *OutRecords = MoveTemp(Records);
		return Hash;
	}

	const FToolRecord* FindTool(const TArray<FToolRecord>& Records, const FString& Qualified)
	{
		for (const FToolRecord& Record : Records)
			if (Record.Qualified.Equals(Qualified, ESearchCase::CaseSensitive)) return &Record;
		return nullptr;
	}

	bool AssetExists(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty()) return false;
		FAssetData Asset;
		if (FindAssetData(AssetPath, Asset)) return true;
		if (FPackageName::DoesPackageExist(AssetPath)) return true;
		FString AssetName = AssetPath;
		int32 Slash = INDEX_NONE;
		if (AssetName.FindLastChar(TEXT('/'), Slash)) AssetName = AssetName.RightChop(Slash + 1);
		return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName)).ResolveObject() != nullptr;
	}

	FString RevisionForAsset(const FString& AssetPath)
	{
		FString Filename;
		if (FPackageName::DoesPackageExist(AssetPath, &Filename))
		{
			const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*Filename);
			return FUeremcpContentHash::Sha256Prefixed(AssetPath + TEXT("|") + Stamp.ToIso8601());
		}
		return FUeremcpContentHash::Sha256Prefixed(TEXT("missing|") + AssetPath);
	}

	TSharedPtr<FJsonObject> MakeEnvelope(const FString& Action, const TSharedPtr<FJsonObject>& Arguments)
	{
		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetStringField(TEXT("protocol_version"), FUeremcpEnvelope::ProtocolVersion());
		Envelope->SetStringField(TEXT("action"), Action);
		if (Arguments.IsValid())
		{
			for (const auto& Pair : Arguments->Values) Envelope->SetField(FString(Pair.Key), Pair.Value);
		}
		return Envelope;
	}

	bool BindAssetReference(const TSharedPtr<FJsonObject>& Properties, const FString& Field, const FString& AssetPath,
		TSharedPtr<FJsonObject>& Invocation, TArray<FString>& Missing)
	{
		const FString Normalized = Lower(Field);
		const bool bSemanticAssetField = Normalized == TEXT("system") || Normalized == TEXT("niagarasystem")
			|| Normalized == TEXT("asset") || Normalized == TEXT("assetref") || Normalized == TEXT("assetpath")
			|| Normalized == TEXT("targetasset") || Normalized == TEXT("blueprint")
			|| Normalized == TEXT("material") || Normalized == TEXT("materialinstance") || Normalized == TEXT("dataasset");
		if (!bSemanticAssetField) return false;
		const TSharedPtr<FJsonObject>* Schema = nullptr;
		if (!Properties->TryGetObjectField(Field, Schema) || !Schema || !Schema->IsValid()) return false;
		const TSharedPtr<FJsonObject>* NestedProperties = nullptr;
		if (!(*Schema)->TryGetObjectField(TEXT("properties"), NestedProperties) || !NestedProperties) return false;
		const TSharedPtr<FJsonObject>* RefPathSchema = nullptr;
		if ((*NestedProperties)->TryGetObjectField(TEXT("refPath"), RefPathSchema) && RefPathSchema)
		{
			TSharedPtr<FJsonObject> Reference = MakeShared<FJsonObject>();
			Reference->SetStringField(TEXT("refPath"), AssetPath);
			Invocation->SetObjectField(Field, Reference);
			return true;
		}
		return false;
	}

	void BindRequiredAssetArguments(const FToolRecord& Record, const FString& AssetPath,
		TSharedPtr<FJsonObject>& Invocation, TArray<FString>& Missing)
	{
		if (!Record.InputSchema.IsValid()) return;
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (!Record.InputSchema->TryGetObjectField(TEXT("properties"), Properties) || !Properties) return;
		const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
		if (!Record.InputSchema->TryGetArrayField(TEXT("required"), Required) || !Required) return;
		for (const TSharedPtr<FJsonValue>& RequiredValue : *Required)
		{
			const FString Field = RequiredValue->AsString();
			if (Field == TEXT("protocol_version") || Field == TEXT("action")) continue;
			if (Field == TEXT("target") || Field == TEXT("specification") || Field == TEXT("options")) continue;
			if (!AssetPath.IsEmpty() && BindAssetReference(*Properties, Field, AssetPath, Invocation, Missing)) continue;
			const TSharedPtr<FJsonObject>* FieldSchema = nullptr;
			if ((*Properties)->TryGetObjectField(Field, FieldSchema) && FieldSchema)
			{
				const TSharedPtr<FJsonObject>* NestedProperties = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* NestedRequired = nullptr;
				if ((*FieldSchema)->TryGetObjectField(TEXT("properties"), NestedProperties)
					&& (*FieldSchema)->TryGetArrayField(TEXT("required"), NestedRequired)
					&& NestedRequired)
				{
					TSharedPtr<FJsonObject> Nested = MakeShared<FJsonObject>();
					for (const TSharedPtr<FJsonValue>& NestedValue : *NestedRequired)
					{
						const FString NestedField = NestedValue->AsString();
						if (!AssetPath.IsEmpty() && (NestedField == TEXT("system") || NestedField == TEXT("niagaraSystem")))
						{
							TSharedPtr<FJsonObject> SystemRef = MakeShared<FJsonObject>();
							SystemRef->SetStringField(TEXT("refPath"), AssetPath);
							Nested->SetObjectField(NestedField, SystemRef);
						}
						else
						{
							Missing.Add(Field + TEXT(".") + NestedField);
						}
					}
					Invocation->SetObjectField(Field, Nested);
					continue;
				}
			}
			Missing.Add(Field);
		}
		// A target is not valid merely because the outer schema has protocol fields.
		// Any asset-scoped action without a bound resource must ask for one.
		if (AssetPath.IsEmpty() && Missing.Num() == 0)
		{
			Missing.Add(TEXT("target.asset_path"));
		}
	}

	TSharedPtr<FJsonObject> Resource(const FString& Path, const FString& Source)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		FAssetData Asset;
		const bool bFound = FindAssetData(Path, Asset);
		Out->SetStringField(TEXT("resource_id"), TEXT("asset_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Out->SetStringField(TEXT("asset_path"), Path);
		Out->SetStringField(TEXT("asset_type"), bFound ? AssetTypeForData(Asset) : AssetTypeForPath(Path));
		Out->SetStringField(TEXT("revision"), RevisionForAsset(Path));
		TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetBoolField(TEXT("exists"), bFound || AssetExists(Path));
		if (bFound)
		{
			Summary->SetStringField(TEXT("asset_name"), Asset.AssetName.ToString());
			Summary->SetStringField(TEXT("asset_class"), Asset.AssetClassPath.ToString());
			Summary->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
			Summary->SetStringField(TEXT("object_path"), Asset.GetObjectPathString());
			Summary->SetStringField(TEXT("summary_kind"), TEXT("asset_registry_compact"));
		}
		Out->SetObjectField(TEXT("summary"), Summary);
		Out->SetStringField(TEXT("source"), Source);
		Out->SetNumberField(TEXT("confidence"), AssetExists(Path) ? 1.0 : 0.35);
		return Out;
	}

	void LogDiagnostic(const FString& ContextId, const FString& ActionId, const FString& Tool, const FString& AssetPath,
		int32 InputBytes, int32 OutputBytes, double DurationMs, const FString& Status, const FString& Error,
		const FString& Risk, bool bChanged, bool bSaved, const FString& Before, const FString& After,
		const FString& ContractHash, const FString& RegistryHash, bool bReused, bool bContinuation,
		int32 InternalBridgeCalls, int32 TemporaryPythonJsonFiles)
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ForgeMeta/MCPDiagnostics"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
		Row->SetStringField(TEXT("context_id"), ContextId);
		Row->SetStringField(TEXT("action_id"), ActionId);
		Row->SetStringField(TEXT("tool_name"), Tool);
		Row->SetStringField(TEXT("asset_resource_path"), AssetPath);
		Row->SetNumberField(TEXT("input_payload_size"), InputBytes);
		Row->SetNumberField(TEXT("output_payload_size"), OutputBytes);
		Row->SetNumberField(TEXT("duration_ms"), DurationMs);
		Row->SetStringField(TEXT("status"), Status);
		Row->SetStringField(TEXT("error"), Error);
		Row->SetStringField(TEXT("risk"), Risk);
		Row->SetBoolField(TEXT("asset_changed"), bChanged);
		Row->SetBoolField(TEXT("asset_saved"), bSaved);
		Row->SetStringField(TEXT("revision_before"), Before);
		Row->SetStringField(TEXT("revision_after"), After);
		Row->SetStringField(TEXT("contract_hash"), ContractHash);
		Row->SetStringField(TEXT("registry_hash"), RegistryHash);
		Row->SetBoolField(TEXT("immediately_reused"), bReused);
		Row->SetBoolField(TEXT("continuation_returned"), bContinuation);
		Row->SetNumberField(TEXT("internal_bridge_call_count"), InternalBridgeCalls);
		Row->SetNumberField(TEXT("temporary_python_json_files"), TemporaryPythonJsonFiles);
		FString Json = JsonObjectToString(Row) + TEXT("\n");
		FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("prepared_actions.jsonl")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}

	int32 DetectTemporaryPythonJsonFiles()
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ForgeMeta/MCPDiagnostics"));
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*.py"), true, false);
		TArray<FString> JsonFiles;
		IFileManager::Get().FindFilesRecursive(JsonFiles, *Directory, TEXT("*.json"), true, false);
		return Files.Num() + JsonFiles.Num();
	}

	TSharedPtr<FJsonObject> Response(const FString& RequestId, const FString& Status, const FString& Summary,
		const TSharedPtr<FJsonObject>& Result = nullptr)
	{
		FUeremcpResponse Envelope;
		Envelope.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
		Envelope.RequestId = RequestId;
		Envelope.Status = Status;
		Envelope.Summary = Summary;
		Envelope.Metrics.McpRoundTrips = 1;
		Envelope.Metrics.InternalOperations = 0;
		TSharedPtr<FJsonObject> Outer;
		const FString Json = FUeremcpEnvelope::SerializeResponse(Envelope);
		FString Ignored;
		ParseObject(Json, Outer, Ignored);
		if (Result.IsValid()) Outer->SetObjectField(TEXT("result"), Result);
		return Outer;
	}

	bool GetInput(const FString& Json, TSharedPtr<FJsonObject>& Root, TSharedPtr<FJsonObject>& Spec, FString& RequestId, FString& Error)
	{
		if (!ParseObject(Json, Root, Error)) return false;
		RequestId = StringField(Root, TEXT("request_id"));
		Spec = SpecFor(Root);
		return true;
	}

	bool IsExpired(const FPreparedAction& Action)
	{
		return FDateTime::UtcNow() > Action.ExpiresAtUtc;
	}

	bool MergeOverrides(const FPreparedAction& Action, const TSharedPtr<FJsonObject>& Overrides,
		TSharedPtr<FJsonObject>& Out, FString& Error)
	{
		Out = MakeEnvelope(CamelToSnake(Action.Tool), Action.Arguments);
		if (!Overrides.IsValid()) return true;
		for (const auto& Pair : Overrides->Values)
		{
			const FString Key(Pair.Key);
			if (Key != TEXT("target") && Key != TEXT("specification") && Key != TEXT("options"))
			{
				Error = FString::Printf(TEXT("invalid override field '%s'"), *Key);
				return false;
			}
			if (Pair.Value->Type != EJson::Object)
			{
				Error = FString::Printf(TEXT("override '%s' must be an object"), *Key);
				return false;
			}
			const TSharedPtr<FJsonObject> Incoming = Pair.Value->AsObject();
			if (Key == TEXT("target"))
			{
				for (const auto& TargetPair : Incoming->Values)
				{
					if (FString(TargetPair.Key) != TEXT("asset_path"))
					{
						Error = TEXT("only target.asset_path may be overridden");
						return false;
					}
					FString Path;
					if (!TargetPair.Value->TryGetString(Path) || !Action.ScopeAssetPaths.Contains(Path))
					{
						Error = TEXT("override target.asset_path is outside the original resource scope");
						return false;
					}
				}
			}
			if (Key == TEXT("specification"))
			{
				for (const auto& SpecPair : Incoming->Values)
				{
					if (Action.SpecificationFields.Num() > 0 && !Action.SpecificationFields.Contains(FString(SpecPair.Key)))
					{
						Error = FString::Printf(TEXT("specification.%s is not an allowed prepared field"), *FString(SpecPair.Key));
						return false;
					}
				}
			}
			if (Key == TEXT("options"))
			{
				for (const auto& OptionPair : Incoming->Values)
				{
					if (!IsAllowedOption(FString(OptionPair.Key)))
					{
						Error = FString::Printf(TEXT("options.%s is not an allowed prepared field"), *FString(OptionPair.Key));
						return false;
					}
				}
			}
			const TSharedPtr<FJsonObject>* Existing = nullptr;
			if (Out->TryGetObjectField(Key, Existing) && Existing && Existing->IsValid())
			{
				for (const auto& IncomingPair : Incoming->Values) (*Existing)->SetField(FString(IncomingPair.Key), IncomingPair.Value);
			}
			else
			{
				Out->SetObjectField(Key, Incoming);
			}
		}
		return true;
	}

	bool ExecuteUnderlying(const FToolRecord& Record, const FString& Json, FString& OutResult, FString& Error)
	{
		UToolCallAsyncResultString* Pending = UToolsetRegistry::ExecuteTool(Record.Toolset, Record.Tool, Json);
		if (!Pending)
		{
			Error = TEXT("ToolsetRegistry::ExecuteTool returned null");
			return false;
		}
		const double Deadline = FPlatformTime::Seconds() + 60.0;
		while (!Pending->bIsComplete && FPlatformTime::Seconds() < Deadline)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		if (!Pending->bIsComplete)
		{
			Error = TEXT("prepared underlying tool timed out");
			return false;
		}
		if (!Pending->Error.IsEmpty())
		{
			Error = Pending->Error;
			return false;
		}
		OutResult = Pending->GetValueAsJsonString();
		return !OutResult.IsEmpty();
	}

	TSharedPtr<FJsonObject> SerializePreparedAction(const FPreparedAction& Action, const FToolRecord& Record, const FString& Provenance)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("action_id"), Action.ActionId);
		Out->SetStringField(TEXT("context_id"), Action.ContextId);
		Out->SetStringField(TEXT("status"), Action.ContinuationKind == TEXT("blocked") || Action.MissingArguments.Num() > 0 || Action.AssetPath.IsEmpty() || !Action.bResourceExisted ? TEXT("needs_input") : TEXT("ready"));
		Out->SetStringField(TEXT("tool"), Action.Tool);
		Out->SetStringField(TEXT("purpose"), Record.Description.Left(180));
		Out->SetStringField(TEXT("domain"), Action.Domain);
		TArray<TSharedPtr<FJsonValue>> Lifecycle;
		Lifecycle.Add(MakeShared<FJsonValueString>(Action.Lifecycle));
		Out->SetArrayField(TEXT("lifecycle"), Lifecycle);
		Out->SetStringField(TEXT("risk"), Action.Risk);
		Out->SetStringField(TEXT("contract_hash"), Action.ContractHash);
		Out->SetStringField(TEXT("registry_hash"), Action.RegistryHash);
		if (!Action.ContinuationKind.IsEmpty()) Out->SetStringField(TEXT("continuation_type"), Action.ContinuationKind);
		Out->SetNumberField(TEXT("confidence"), Action.Confidence);
		Out->SetStringField(TEXT("recommendation_reason"), Action.RecommendationReason);
		Out->SetObjectField(TEXT("arguments"), Action.Arguments);
		TArray<TSharedPtr<FJsonValue>> Missing;
		for (const FString& Field : Action.MissingArguments) Missing.Add(MakeShared<FJsonValueString>(Field));
		Out->SetArrayField(TEXT("missing_arguments"), Missing);
		TSharedPtr<FJsonObject> Binding = MakeShared<FJsonObject>();
		Binding->SetStringField(TEXT("target.asset_path"), Provenance);
		Out->SetObjectField(TEXT("binding_provenance"), Binding);
		TSharedPtr<FJsonObject> TargetResource = MakeShared<FJsonObject>();
		TargetResource->SetStringField(TEXT("resource_id"), Action.ResourceId);
		TargetResource->SetStringField(TEXT("asset_path"), Action.AssetPath);
		TargetResource->SetStringField(TEXT("asset_type"), AssetTypeForPath(Action.AssetPath));
		TargetResource->SetStringField(TEXT("expected_revision"), Action.ExpectedRevision);
		Out->SetObjectField(TEXT("target_resource"), TargetResource);
		Out->SetStringField(TEXT("expected_revision"), Action.ExpectedRevision);
		TArray<TSharedPtr<FJsonValue>> SideEffects;
		SideEffects.Add(MakeShared<FJsonValueString>(Action.Risk));
		Out->SetArrayField(TEXT("side_effects"), SideEffects);
		Out->SetBoolField(TEXT("confirmation_required"), Action.bConfirmationRequired);
		Out->SetBoolField(TEXT("dry_run_available"), true);
		Out->SetArrayField(TEXT("depends_on"), {});
		Out->SetStringField(TEXT("expected_result"), TEXT("Structured UEREMCP response with bounded prepared continuations"));
		Out->SetStringField(TEXT("expires_at"), Action.ExpiresAt);
		return Out;
	}

	TArray<TSharedPtr<FJsonObject>> Continuations(const FPreparedAction& Action, const FString& Registry, const TArray<FToolRecord>& Records)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		if (Action.AssetPath.IsEmpty() || !Action.bResourceExisted) return Out;
		if (Action.Lifecycle == TEXT("inspect") || Action.Lifecycle == TEXT("discover") || Action.Lifecycle == TEXT("validate")
			|| Action.Lifecycle == TEXT("modify") || Action.Lifecycle == TEXT("preview"))
		{
			TArray<FString> DesiredNames;
			if (Action.Lifecycle == TEXT("inspect") || Action.Lifecycle == TEXT("discover"))
			{
				DesiredNames = {TEXT("Validate"), TEXT("Preview"), TEXT("Adapt"), TEXT("Patch"), TEXT("Submit")};
			}
			else if (Action.Lifecycle == TEXT("modify"))
			{
				DesiredNames = {TEXT("Validate"), TEXT("Preview"), TEXT("Save")};
			}
			else
			{
				DesiredNames = {TEXT("Validate"), TEXT("Preview"), TEXT("Capture"), TEXT("Save")};
			}
			for (const FString& Desired : DesiredNames)
			{
				const FToolRecord* Candidate = nullptr;
				auto IsRelevant = [&Action, &Desired](const FToolRecord& Record)
				{
					const bool bValidationFamily = Desired.Equals(TEXT("Validate"), ESearchCase::IgnoreCase)
						|| Desired.Equals(TEXT("Preview"), ESearchCase::IgnoreCase)
						|| Desired.Equals(TEXT("Capture"), ESearchCase::IgnoreCase);
					const bool bSameDomain = Record.Domain.Equals(Action.Domain, ESearchCase::IgnoreCase)
						|| (bValidationFamily && Record.Domain.Equals(TEXT("validation"), ESearchCase::IgnoreCase));
					if (!bSameDomain || !Record.bMetadataExplicit) return false;
					if (Desired.Equals(TEXT("Validate"), ESearchCase::IgnoreCase)) return Record.Lifecycle == TEXT("validate") || Record.Tool.Contains(Desired, ESearchCase::IgnoreCase);
					if (Desired.Equals(TEXT("Preview"), ESearchCase::IgnoreCase) || Desired.Equals(TEXT("Capture"), ESearchCase::IgnoreCase)) return Record.Lifecycle == TEXT("preview") || Record.Tool.Contains(Desired, ESearchCase::IgnoreCase);
					if (Desired.Equals(TEXT("Save"), ESearchCase::IgnoreCase)) return Record.Lifecycle == TEXT("save") || Record.Tool.Contains(Desired, ESearchCase::IgnoreCase);
					return Record.Lifecycle == TEXT("modify") || Record.Tool.Contains(Desired, ESearchCase::IgnoreCase);
				};
				for (const FToolRecord& Record : Records)
				{
					if (!IsRelevant(Record)) continue;
					if (Record.Tool.Equals(Desired, ESearchCase::IgnoreCase)) { Candidate = &Record; break; }
				}
				if (!Candidate)
				{
					for (const FToolRecord& Record : Records)
					{
						if (IsRelevant(Record) && Record.Tool.Contains(Desired, ESearchCase::IgnoreCase)) { Candidate = &Record; break; }
					}
				}
				if (!Candidate || Candidate->Qualified.Equals(Action.Tool, ESearchCase::CaseSensitive)) continue;
				const bool bBlockedByRisk = !RiskWithin(Candidate->Risk, Action.RiskCeiling);
				FPreparedAction NextAction;
				NextAction.ActionId = TEXT("action_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
				NextAction.ContextId = Action.ContextId;
				NextAction.Goal = Action.Goal;
				NextAction.Tool = Candidate->Qualified;
				NextAction.Domain = Candidate->Domain;
				NextAction.Risk = Candidate->Risk;
				NextAction.RiskCeiling = Action.RiskCeiling;
				NextAction.Lifecycle = Candidate->Lifecycle;
				NextAction.RegistryHash = Registry;
				NextAction.ContractHash = Candidate->ContractHash;
				NextAction.AssetPath = Action.AssetPath;
				NextAction.ResourceId = Action.ResourceId;
				NextAction.ExpectedRevision = RevisionForAsset(Action.AssetPath);
				NextAction.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(10);
				NextAction.ExpiresAt = NextAction.ExpiresAtUtc.ToIso8601();
				NextAction.ScopeAssetPaths = Action.ScopeAssetPaths;
				NextAction.SpecificationFields = Candidate->SpecificationFields;
				NextAction.bResourceExisted = AssetExists(Action.AssetPath);
				NextAction.bConfirmationRequired = Candidate->Risk != TEXT("read_only");
				NextAction.ContinuationKind = Candidate->Risk != TEXT("read_only") ? TEXT("confirmation_required") : (Out.Num() == 0 ? TEXT("recommended_next") : TEXT("alternative"));
				NextAction.Confidence = Candidate->Risk != TEXT("read_only") ? 0.65 : (Out.Num() == 0 ? 0.92 : 0.78);
				NextAction.RecommendationReason = Candidate->Risk != TEXT("read_only")
					? TEXT("The same verified resource and revision are retained, but this continuation is mutating and requires explicit confirmation.")
					: TEXT("The same verified resource and expected revision satisfy this read-only continuation.");
				if (bBlockedByRisk)
				{
					NextAction.ContinuationKind = TEXT("blocked");
					NextAction.Confidence = 0.96;
					NextAction.RecommendationReason = TEXT("This valid next operation exceeds the original risk ceiling; raise the ceiling and prepare a new action explicitly.");
				}
				NextAction.Arguments = MakeShared<FJsonObject>();
				NextAction.Arguments->SetStringField(TEXT("protocol_version"), FUeremcpEnvelope::ProtocolVersion());
				NextAction.Arguments->SetStringField(TEXT("action"), CamelToSnake(Candidate->Tool));
				TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
				Target->SetStringField(TEXT("asset_path"), Action.AssetPath);
				NextAction.Arguments->SetObjectField(TEXT("target"), Target);
				NextAction.Arguments->SetObjectField(TEXT("specification"), MakeShared<FJsonObject>());
				TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
				Options->SetBoolField(TEXT("dry_run"), Candidate->Risk != TEXT("read_only"));
				NextAction.Arguments->SetObjectField(TEXT("options"), Options);
				BindRequiredAssetArguments(*Candidate, NextAction.AssetPath, NextAction.Arguments, NextAction.MissingArguments);
				if (Candidate->Lifecycle == TEXT("modify") && NextAction.MissingArguments.Num() == 0)
				{
					NextAction.MissingArguments.Add(TEXT("specification"));
				}
				if (bBlockedByRisk) NextAction.MissingArguments.Add(TEXT("risk_ceiling"));
				if (NextAction.MissingArguments.Num() > 0 && !bBlockedByRisk) NextAction.ContinuationKind = TEXT("needs_input");
			{
					FScopeLock Lock(&StoreMutex());
					Actions().Add(NextAction.ActionId, NextAction);
				}
				Out.Add(SerializePreparedAction(NextAction, *Candidate, TEXT("previous_result.resource")));
				if (Out.Num() == 3) break;
			}
		}
		return Out;
	}
}

FString FUeremcpCapabilityService::ResolveAndPrepare(const FString& RequestJson)
{
	using namespace UeremcpCapabilityInternal;
	TSharedPtr<FJsonObject> Root, Spec;
	FString RequestId, Error;
	if (!GetInput(RequestJson, Root, Spec, RequestId, Error)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), Error));
	FString Goal = StringField(Spec, TEXT("goal"));
	if (Goal.IsEmpty()) Goal = StringField(Spec, TEXT("intent"));
	if (Goal.IsEmpty()) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("goal is required")));
	const FString RiskCeiling = StringField(Spec, TEXT("risk_ceiling"), TEXT("read_only")).ToLower();
	if (!RiskWithin(RiskCeiling, TEXT("destructive"))) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("invalid risk_ceiling")));
	const int32 MaxActions = IntField(Spec, TEXT("max_actions"), 5);
	FString ExpectedRegistry = StringField(Spec, TEXT("registry_hash"));
	TArray<FToolRecord> Records;
	const FString LiveRegistry = CurrentRegistryHash(&Records);
	if (LiveRegistry.IsEmpty()) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("live registry unavailable")));
	if (!ExpectedRegistry.IsEmpty() && !ExpectedRegistry.Equals(LiveRegistry, ESearchCase::IgnoreCase))
		return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("registry hash mismatch; refresh live capabilities")));

	TArray<FString> AssetPaths;
	TSet<FString> AllowedDomains;
	FString SearchQuery;
	FString SearchRoot = TEXT("/Game");
	FString RequestedAssetType;
	TArray<FString> WarningStrings;
	const TSharedPtr<FJsonObject>* Scope = nullptr;
	if (Spec->TryGetObjectField(TEXT("scope"), Scope) && Scope)
	{
		const TArray<TSharedPtr<FJsonValue>>* Paths = nullptr;
		if ((*Scope)->TryGetArrayField(TEXT("asset_paths"), Paths) && Paths)
			for (const TSharedPtr<FJsonValue>& Value : *Paths) AssetPaths.Add(Value->AsString());
		const TArray<TSharedPtr<FJsonValue>>* Domains = nullptr;
		if ((*Scope)->TryGetArrayField(TEXT("allowed_domains"), Domains) && Domains)
			for (const TSharedPtr<FJsonValue>& Value : *Domains) AllowedDomains.Add(Lower(Value->AsString()));
		const TSharedPtr<FJsonObject>* Search = nullptr;
		if ((*Scope)->TryGetObjectField(TEXT("asset_search"), Search) && Search)
		{
			SearchQuery = StringField(*Search, TEXT("query"));
			SearchRoot = StringField(*Search, TEXT("search_root"), SearchRoot);
			RequestedAssetType = StringField(*Search, TEXT("asset_type"));
		}
	}
	if (AssetPaths.Num() == 0)
	{
		const FString AssetPath = StringField(Spec, TEXT("asset_path"));
		if (!AssetPath.IsEmpty()) AssetPaths.Add(AssetPath);
	}
	if (SearchQuery.IsEmpty()) SearchQuery = StringField(Spec, TEXT("asset_name"));
	if (SearchQuery.IsEmpty()) SearchQuery = StringField(Spec, TEXT("query"));
	if (SearchRoot == TEXT("/Game")) SearchRoot = StringField(Spec, TEXT("search_root"), SearchRoot);
	if (RequestedAssetType.IsEmpty()) RequestedAssetType = StringField(Spec, TEXT("asset_type"));
	if (AssetPaths.Num() == 0 && SearchQuery.IsEmpty())
	{
		TArray<FString> GoalWords;
		Goal.ParseIntoArrayWS(GoalWords);
		for (const FString& Word : GoalWords)
		{
			const FString Candidate = Word.TrimStartAndEnd();
			const FString LowerCandidate = Candidate.ToLower();
			if (LowerCandidate.StartsWith(TEXT("ns_")) || LowerCandidate.StartsWith(TEXT("mi_"))
				|| LowerCandidate.StartsWith(TEXT("bp_")) || LowerCandidate.StartsWith(TEXT("da_")))
			{
				SearchQuery = Candidate;
				break;
			}
		}
	}
	if (AssetPaths.Num() == 0 && !SearchQuery.IsEmpty())
	{
		TArray<FString> FoundPaths;
		if (SearchAssets(SearchQuery, SearchRoot, RequestedAssetType, FoundPaths))
		{
			AssetPaths = FoundPaths;
		}
		else
		{
			WarningStrings.Add(FString::Printf(TEXT("No assets matched '%s' under %s; target-bound actions require input."), *SearchQuery, *SearchRoot));
		}
	}

	const FString ContextId = StringField(Spec, TEXT("context_id"), TEXT("ctx_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
	TArray<TSharedPtr<FJsonValue>> Resources;
	TMap<FString, FString> ResourceIds;
	for (const FString& Path : AssetPaths)
	{
		const TSharedPtr<FJsonObject> ResourceObject = Resource(Path, SearchQuery.IsEmpty() ? TEXT("user_request") : TEXT("search"));
		ResourceIds.Add(Path, StringField(ResourceObject, TEXT("resource_id")));
		Resources.Add(MakeShared<FJsonValueObject>(ResourceObject));
	}

	struct FScore { double Value; int32 Index; };
	FString PrimaryAssetType;
	if (Resources.Num() > 0 && Resources[0].IsValid() && Resources[0]->Type == EJson::Object)
	{
		PrimaryAssetType = StringField(Resources[0]->AsObject(), TEXT("asset_type"));
	}
	TArray<FScore> Scores;
	const FString Query = Lower(Goal);
	TArray<FString> QueryTokens;
	Query.ParseIntoArray(QueryTokens, TEXT(" "), true);
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FToolRecord& Record = Records[Index];
		if (!RiskWithin(Record.Risk, RiskCeiling)) continue;
		if (AllowedDomains.Num() > 0 && !AllowedDomains.Contains(Record.Domain)) continue;
		if (AssetPaths.Num() > 0 && !PrimaryAssetType.IsEmpty() && Record.AssetTypes.Num() > 0)
		{
			bool bAssetTypeMatches = false;
			for (const FString& AssetType : Record.AssetTypes)
			{
				if (PrimaryAssetType.Equals(AssetType, ESearchCase::IgnoreCase))
				{
					bAssetTypeMatches = true;
					break;
				}
			}
			if (!bAssetTypeMatches) continue;
		}
		const FString Blob = Lower(Record.Qualified + TEXT(" ") + Record.Description + TEXT(" ") + Record.Domain + TEXT(" ") + Record.Lifecycle);
		double Score = 0.0;
		for (const FString& Token : QueryTokens) if (Blob.Contains(Token)) Score += 1.0;
		if (AssetPaths.Num() > 0 && (Record.Lifecycle == TEXT("inspect") || Record.Lifecycle == TEXT("validate") || Record.Lifecycle == TEXT("preview"))) Score += 2.0;
		if (Query.Contains(TEXT("niagara")) && Record.Domain == TEXT("niagara")) Score += 4.0;
		if (Query.Contains(TEXT("inspect")) && Record.Lifecycle == TEXT("inspect")) Score += 4.0;
		if (Score > 0.0) Scores.Add({Score, Index});
	}
	Scores.Sort([](const FScore& A, const FScore& B) { return A.Value > B.Value; });
	TArray<TSharedPtr<FJsonValue>> Prepared;
	for (int32 Rank = 0; Rank < Scores.Num() && Prepared.Num() < MaxActions; ++Rank)
	{
		const FToolRecord& Record = Records[Scores[Rank].Index];
		FPreparedAction Action;
		Action.ActionId = TEXT("action_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Action.ContextId = ContextId;
		Action.Goal = Goal;
		Action.Tool = Record.Qualified;
		Action.Domain = Record.Domain;
		Action.Risk = Record.Risk;
		Action.RiskCeiling = RiskCeiling;
		Action.Lifecycle = Record.Lifecycle;
		Action.RegistryHash = LiveRegistry;
		Action.ContractHash = Record.ContractHash;
		Action.ScopeAssetPaths = AssetPaths;
		Action.SpecificationFields = Record.SpecificationFields;
		Action.AssetPath = AssetPaths.Num() > 0 ? AssetPaths[0] : FString();
		Action.ResourceId = Action.AssetPath.IsEmpty() ? FString() : ResourceIds.FindRef(Action.AssetPath);
		Action.bResourceExisted = Action.AssetPath.IsEmpty() ? false : AssetExists(Action.AssetPath);
		Action.ExpectedRevision = Action.AssetPath.IsEmpty() ? FString() : RevisionForAsset(Action.AssetPath);
		Action.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(10);
		Action.ExpiresAt = Action.ExpiresAtUtc.ToIso8601();
		TSharedPtr<FJsonObject> Invocation = MakeShared<FJsonObject>();
		Invocation->SetStringField(TEXT("protocol_version"), FUeremcpEnvelope::ProtocolVersion());
		Invocation->SetStringField(TEXT("action"), CamelToSnake(Record.Tool));
		if (!Action.AssetPath.IsEmpty())
		{
			TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
			Target->SetStringField(TEXT("asset_path"), Action.AssetPath);
			Invocation->SetObjectField(TEXT("target"), Target);
		}
		Invocation->SetObjectField(TEXT("specification"), MakeShared<FJsonObject>());
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), Record.Risk != TEXT("read_only"));
		Options->SetStringField(TEXT("response_detail"), StringField(Spec, TEXT("response_detail"), TEXT("summary")));
		Invocation->SetObjectField(TEXT("options"), Options);
		BindRequiredAssetArguments(Record, Action.AssetPath, Invocation, Action.MissingArguments);
		Action.Arguments = Invocation;
		Action.bConfirmationRequired = Record.Risk != TEXT("read_only");
		Action.ContinuationKind = TEXT("recommended_next");
		Action.Confidence = Action.AssetPath.IsEmpty() || !Action.bResourceExisted ? 0.2 : (Record.bMetadataExplicit ? 0.92 : 0.65);
		Action.RecommendationReason = Action.AssetPath.IsEmpty() || !Action.bResourceExisted
			? TEXT("This candidate matches the goal but has no verified target resource; provide an asset binding before execution.")
			: TEXT("This candidate matches the goal and is bound to the verified resource and expected revision.");
		if (Action.MissingArguments.Num() > 0 || Action.AssetPath.IsEmpty() || !Action.bResourceExisted)
			Action.ContinuationKind = TEXT("needs_input");
		{
			FScopeLock Lock(&StoreMutex());
			Actions().Add(Action.ActionId, Action);
		}
		const FString Provenance = AssetPaths.Num() > 0
			? (SearchQuery.IsEmpty() ? TEXT("user_request.scope.asset_paths") : TEXT("server_asset_search"))
			: TEXT("unbound");
		Prepared.Add(MakeShared<FJsonValueObject>(SerializePreparedAction(Action, Record, Provenance)));
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("context_id"), ContextId);
	Result->SetStringField(TEXT("registry_hash"), LiveRegistry);
	Result->SetArrayField(TEXT("resources"), Resources);
	Result->SetArrayField(TEXT("recommended_actions"), Prepared);
	TArray<TSharedPtr<FJsonValue>> Warnings;
	for (const FString& Warning : WarningStrings) Warnings.Add(MakeShared<FJsonValueString>(Warning));
	Result->SetArrayField(TEXT("warnings"), Warnings);
	Result->SetStringField(TEXT("expires_at"), (FDateTime::UtcNow() + FTimespan::FromMinutes(10)).ToIso8601());
	const FString FinalJson = JsonObjectToString(Response(RequestId, TEXT("prepared"), TEXT("Prepared bounded, server-owned capability actions"), Result));
	LogDiagnostic(ContextId, FString(), TEXT("UeremcpCore.UeremcpReferenceToolset.ResolveAndPrepare"), AssetPaths.Num() > 0 ? AssetPaths[0] : FString(), RequestJson.Len(), FinalJson.Len(), 0.0, TEXT("prepared"), FString(), RiskCeiling, false, false, FString(), FString(), FString(), LiveRegistry, false, Prepared.Num() > 0, 0, DetectTemporaryPythonJsonFiles());
	return FinalJson;
}

FString FUeremcpCapabilityService::GetCapabilityContract(const FString& RequestJson)
{
	using namespace UeremcpCapabilityInternal;
	TSharedPtr<FJsonObject> Root, Spec;
	FString RequestId, Error;
	if (!GetInput(RequestJson, Root, Spec, RequestId, Error)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), Error));
	const FString Tool = StringField(Spec, TEXT("tool"));
	if (Tool.IsEmpty()) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("tool is required")));
	TArray<FToolRecord> Records;
	const FString Registry = CurrentRegistryHash(&Records);
	const FToolRecord* Found = FindTool(Records, Tool);
	if (!Found) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("tool is not in the live registry")));
	const FString Expected = StringField(Spec, TEXT("registry_hash"));
	if (!Expected.IsEmpty() && !Expected.Equals(Registry, ESearchCase::IgnoreCase)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("registry hash mismatch")));
	TSharedPtr<FJsonObject> Contract = CompactContract(*Found);
	Contract->SetStringField(TEXT("tool"), Found->Qualified);
	Contract->SetStringField(TEXT("domain"), Found->Domain);
	Contract->SetStringField(TEXT("lifecycle"), Found->Lifecycle);
	Contract->SetStringField(TEXT("risk"), Found->Risk);
	Contract->SetStringField(TEXT("registry_hash"), Registry);
	if (StringField(Spec, TEXT("detail"), TEXT("compact")).Equals(TEXT("full"), ESearchCase::IgnoreCase) && Found->InputSchema.IsValid()) Contract->SetObjectField(TEXT("input_schema"), Found->InputSchema);
	return JsonObjectToString(Response(RequestId, TEXT("no_change_required"), TEXT("Returned one live capability contract"), Contract));
}

FString FUeremcpCapabilityService::SearchCapabilities(const FString& RequestJson)
{
	using namespace UeremcpCapabilityInternal;
	TSharedPtr<FJsonObject> Root, Spec;
	FString RequestId, Error;
	if (!GetInput(RequestJson, Root, Spec, RequestId, Error)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), Error));
	TArray<FToolRecord> Records;
	const FString Registry = CurrentRegistryHash(&Records);
	const FString Query = Lower(StringField(Spec, TEXT("query")));
	const FString Domain = Lower(StringField(Spec, TEXT("domain")));
	const FString Lifecycle = Lower(StringField(Spec, TEXT("lifecycle")));
	const FString Risk = Lower(StringField(Spec, TEXT("risk")));
	const int32 Limit = IntField(Spec, TEXT("max_results"), 10);
	TArray<TSharedPtr<FJsonValue>> Results;
	for (const FToolRecord& Record : Records)
	{
		if (!Domain.IsEmpty() && Record.Domain != Domain) continue;
		if (!Lifecycle.IsEmpty() && Record.Lifecycle != Lifecycle) continue;
		if (!Risk.IsEmpty() && Record.Risk != Risk) continue;
		const FString Blob = Lower(Record.Qualified + TEXT(" ") + Record.Description);
		if (!Query.IsEmpty() && !Blob.Contains(Query)) continue;
		TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
		Hit->SetStringField(TEXT("tool"), Record.Qualified);
		Hit->SetStringField(TEXT("domain"), Record.Domain);
		Hit->SetStringField(TEXT("lifecycle"), Record.Lifecycle);
		Hit->SetStringField(TEXT("risk"), Record.Risk);
		Hit->SetStringField(TEXT("contract_hash"), Record.ContractHash);
		Hit->SetStringField(TEXT("description"), Record.Description.Left(180));
		Results.Add(MakeShared<FJsonValueObject>(Hit));
		if (Results.Num() >= Limit) break;
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("registry_hash"), Registry);
	Result->SetArrayField(TEXT("results"), Results);
	Result->SetBoolField(TEXT("has_more"), Results.Num() >= Limit);
	return JsonObjectToString(Response(RequestId, TEXT("no_change_required"), TEXT("Returned bounded live capability search"), Result));
}

FString FUeremcpCapabilityService::ComputeLiveRegistryHash()
{
	using namespace UeremcpCapabilityInternal;
	return CurrentRegistryHash();
}

FString FUeremcpCapabilityService::ExecutePreparedAction(const FString& RequestJson)
{
	using namespace UeremcpCapabilityInternal;
	const double Started = FPlatformTime::Seconds();
	TSharedPtr<FJsonObject> Root, Spec;
	FString RequestId, Error;
	if (!GetInput(RequestJson, Root, Spec, RequestId, Error)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), Error));
	const FString ActionId = StringField(Root, TEXT("action_id"), StringField(Spec, TEXT("action_id")));
	if (ActionId.IsEmpty()) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("action_id is required")));
	FPreparedAction Action;
	{
		FScopeLock Lock(&StoreMutex());
		const FPreparedAction* Found = Actions().Find(ActionId);
		if (!Found) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("prepared action not found; arbitrary execution is not supported")));
		Action = *Found;
	}
	const FString RequestedContext = StringField(Root, TEXT("context_id"), StringField(Spec, TEXT("context_id")));
	if (!RequestedContext.IsEmpty() && !RequestedContext.Equals(Action.ContextId, ESearchCase::CaseSensitive))
		return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("context_id does not match the server-owned prepared action")));
	if (IsExpired(Action)) return JsonObjectToString(Response(RequestId, TEXT("stale"), TEXT("prepared action expired")));
	TArray<FToolRecord> Records;
	const FString LiveRegistry = CurrentRegistryHash(&Records);
	if (!LiveRegistry.Equals(Action.RegistryHash, ESearchCase::CaseSensitive)) return JsonObjectToString(Response(RequestId, TEXT("stale"), TEXT("registry hash changed; prepare a new action")));
	const FToolRecord* Record = FindTool(Records, Action.Tool);
	if (!Record || !Record->ContractHash.Equals(Action.ContractHash, ESearchCase::CaseSensitive)) return JsonObjectToString(Response(RequestId, TEXT("stale"), TEXT("contract hash changed; retrieve a new contract")));
	if (Action.MissingArguments.Num() > 0)
		return JsonObjectToString(Response(RequestId, TEXT("rejected"), FString::Printf(TEXT("prepared action needs input: %s"), *FString::Join(Action.MissingArguments, TEXT(", ")))));
	if (Action.AssetPath.IsEmpty() || !Action.bResourceExisted)
		return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("prepared action has no verified resource binding; resolve the asset before execution")));
	if (!Action.AssetPath.IsEmpty())
	{
		const bool bExistsNow = AssetExists(Action.AssetPath);
		if (Action.bResourceExisted && !bExistsNow) return JsonObjectToString(Response(RequestId, TEXT("stale"), TEXT("prepared resource no longer exists")));
		if (Action.bResourceExisted && !Action.ExpectedRevision.IsEmpty() && RevisionForAsset(Action.AssetPath) != Action.ExpectedRevision)
			return JsonObjectToString(Response(RequestId, TEXT("stale"), TEXT("asset revision changed; no underlying tool was called")));
	}
	const TSharedPtr<FJsonObject>* Overrides = nullptr;
	TSharedPtr<FJsonObject> OverrideObject;
	if (Root->TryGetObjectField(TEXT("overrides"), Overrides) && Overrides) OverrideObject = *Overrides;
	else if (Spec->TryGetObjectField(TEXT("overrides"), Overrides) && Overrides) OverrideObject = *Overrides;
	TSharedPtr<FJsonObject> Invocation;
	if (!MergeOverrides(Action, OverrideObject, Invocation, Error)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), Error));
	bool bDryRun = true;
	if (Root->HasTypedField<EJson::Boolean>(TEXT("dry_run"))) bDryRun = Root->GetBoolField(TEXT("dry_run"));
	else if (Spec->HasTypedField<EJson::Boolean>(TEXT("dry_run"))) bDryRun = Spec->GetBoolField(TEXT("dry_run"));
	else
	{
		const TSharedPtr<FJsonObject>* RootOptions = nullptr;
		const TSharedPtr<FJsonObject>* SpecOptions = nullptr;
		if (Root->TryGetObjectField(TEXT("options"), RootOptions) && RootOptions && (*RootOptions)->HasTypedField<EJson::Boolean>(TEXT("dry_run")))
			bDryRun = (*RootOptions)->GetBoolField(TEXT("dry_run"));
		else if (Spec->TryGetObjectField(TEXT("options"), SpecOptions) && SpecOptions && (*SpecOptions)->HasTypedField<EJson::Boolean>(TEXT("dry_run")))
			bDryRun = (*SpecOptions)->GetBoolField(TEXT("dry_run"));
	}
	bool bConfirm = false;
	if (Root->HasTypedField<EJson::Boolean>(TEXT("confirm"))) bConfirm = Root->GetBoolField(TEXT("confirm"));
	else if (Spec->HasTypedField<EJson::Boolean>(TEXT("confirm"))) bConfirm = Spec->GetBoolField(TEXT("confirm"));
	if (!RiskWithin(Action.Risk, Action.RiskCeiling)) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("prepared action exceeds original risk ceiling")));
	if (Action.bConfirmationRequired && !bConfirm) return JsonObjectToString(Response(RequestId, TEXT("rejected"), TEXT("confirmation is required for this prepared mutation")));
	const TSharedPtr<FJsonObject>* Options = nullptr;
	if (Invocation->TryGetObjectField(TEXT("options"), Options) && Options)
	{
		(*Options)->SetBoolField(TEXT("dry_run"), bDryRun);
	}

	FString IdempotencyKey = StringField(Root, TEXT("idempotency_key"), StringField(Spec, TEXT("idempotency_key")));
	FString Fingerprint = FUeremcpContentHash::Sha256Prefixed(ActionId + TEXT("|") + JsonObjectToString(Invocation) + TEXT("|") + (bConfirm ? TEXT("confirm") : TEXT("no_confirm")));
	if (!IdempotencyKey.IsEmpty())
	{
		const FUeremcpIdempotencyClaim Claim = FUeremcpIdempotencyStore::Get().Claim(IdempotencyKey, Fingerprint, RequestId);
		if (Claim.Status == EUeremcpIdempotencyClaimStatus::Replay) return Claim.ResponseJson;
		if (Claim.Status != EUeremcpIdempotencyClaimStatus::Acquired) return JsonObjectToString(Response(RequestId, TEXT("rejected"), Claim.Error));
	}

	FString UnderlyingJson;
	FString ExecuteError;
	const bool bExecuted = !bDryRun && ExecuteUnderlying(*Record, JsonObjectToString(Invocation), UnderlyingJson, ExecuteError);
	TSharedPtr<FJsonObject> Underlying;
	if (!UnderlyingJson.IsEmpty())
	{
		FString ParseError;
		ParseObject(UnderlyingJson, Underlying, ParseError);
		if (Underlying.IsValid() && Underlying->HasTypedField<EJson::String>(TEXT("returnValue")))
		{
			FString Nested = Underlying->GetStringField(TEXT("returnValue"));
			TSharedPtr<FJsonObject> NestedObject;
			if (ParseObject(Nested, NestedObject, ParseError)) Underlying = NestedObject;
		}
	}
	if (!bDryRun && !bExecuted)
	{
		if (!IdempotencyKey.IsEmpty()) { FString Ignored; FUeremcpIdempotencyStore::Get().Abandon(IdempotencyKey, Fingerprint, Ignored); }
		LogDiagnostic(Action.ContextId, Action.ActionId, Action.Tool, Action.AssetPath, RequestJson.Len(), 0, (FPlatformTime::Seconds() - Started) * 1000.0, TEXT("failed"), ExecuteError, Action.Risk, false, false, Action.ExpectedRevision, FString(), Action.ContractHash, LiveRegistry, false, false, 1, DetectTemporaryPythonJsonFiles());
		return JsonObjectToString(Response(RequestId, TEXT("failed"), ExecuteError));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("action_id"), Action.ActionId);
	Result->SetStringField(TEXT("tool"), Action.Tool);
	Result->SetBoolField(TEXT("dry_run"), bDryRun);
	if (Underlying.IsValid()) Result->SetObjectField(TEXT("underlying"), Underlying);
	const FString RevisionAfter = Action.AssetPath.IsEmpty() ? FString() : RevisionForAsset(Action.AssetPath);
	Result->SetStringField(TEXT("revision_before"), Action.ExpectedRevision);
	Result->SetStringField(TEXT("revision_after"), RevisionAfter);
	TArray<TSharedPtr<FJsonValue>> Next;
	for (const TSharedPtr<FJsonObject>& Continuation : Continuations(Action, LiveRegistry, Records)) Next.Add(MakeShared<FJsonValueObject>(Continuation));
	Result->SetArrayField(TEXT("recommended_actions"), Next);
	const FString Status = bDryRun ? TEXT("dry_run") : TEXT("completed");
	const FString FinalJson = JsonObjectToString(Response(RequestId, Status, bDryRun ? TEXT("Prepared action dry-run completed; no asset mutation was attempted") : TEXT("Prepared action executed through the live ToolsetRegistry"), Result));
	if (!IdempotencyKey.IsEmpty()) { FString StoreError; FUeremcpIdempotencyStore::Get().Complete(IdempotencyKey, Fingerprint, FinalJson, StoreError); }
	LogDiagnostic(Action.ContextId, Action.ActionId, Action.Tool, Action.AssetPath, RequestJson.Len(), FinalJson.Len(), (FPlatformTime::Seconds() - Started) * 1000.0, Status, FString(), Action.Risk, !bDryRun && Action.Risk != TEXT("read_only"), !bDryRun && Action.Risk == TEXT("saves_asset"), Action.ExpectedRevision, RevisionAfter, Action.ContractHash, LiveRegistry, false, Next.Num() > 0, bDryRun ? 0 : 1, DetectTemporaryPythonJsonFiles());
	return FinalJson;
}

void FUeremcpCapabilityService::ResetForTests()
{
	FScopeLock Lock(&UeremcpCapabilityInternal::StoreMutex());
	UeremcpCapabilityInternal::Actions().Reset();
}
