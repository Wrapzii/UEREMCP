// UEREMCP — find_project_assets (MCP-012 / MCP-016).
//
// Nothing asked the AssetRegistry before this. That is why the router handed out
// /Game/Meshes/SM_Pine, why agents never found PCG trees, and why import looked
// invisible. Role → name patterns live in operation_catalog.json (data-driven),
// not in this file.
//
// API NOTES — read, not recalled:
//   [VERIFIED: Runtime/AssetRegistry/Public/AssetRegistry/AssetRegistryModule.h]
//   [VERIFIED: Runtime/AssetRegistry/Public/AssetRegistry/ARFilter.h] FARFilter
//   [VERIFIED: IAssetRegistry.h:363] GetAssets(FARFilter, TArray<FAssetData>&)
//   [VERIFIED: IAssetRegistry.h:1025] IsLoadingAssets()

#include "UeremcpEnvironmentToolset.h"

#include "UeremcpEnvelope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool CommonPreamble(
		const FString& RequestJson,
		const TCHAR* ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutRejection)
	{
		FString ParseError;
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, ParseError))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				FString(), FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
					*OutRequest.ProtocolVersion, *FUeremcpEnvelope::ProtocolVersion()));
			return false;
		}
		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("%s tool received action '%s'."),
					ExpectedAction, *OutRequest.Action));
			return false;
		}
		return true;
	}

	FString CatalogPath()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UEREMCP")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/IntentRouter/operation_catalog.json"));
		}
		return FString();
	}

	TMap<FString, TArray<FString>> LoadRolePatterns()
	{
		TMap<FString, TArray<FString>> Out;
		// Defaults match the impl doc so an older catalog still works.
		Out.Add(TEXT("foliage.tree"), {TEXT("*Tree*"), TEXT("*Pine*"), TEXT("*Conifer*"), TEXT("*Oak*"), TEXT("PCG_Tree*")});
		Out.Add(TEXT("foliage.grass"), {TEXT("*Grass*"), TEXT("*Fern*"), TEXT("*Bush*")});
		Out.Add(TEXT("structure.wall"), {TEXT("*Wall*"), TEXT("*Fence*"), TEXT("SM_*Wall*")});

		FString Raw;
		const FString Path = CatalogPath();
		if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Raw, *Path))
		{
			return Out;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return Out;
		}
		const TSharedPtr<FJsonObject>* Patterns = nullptr;
		if (!Root->TryGetObjectField(TEXT("asset_role_patterns"), Patterns) || !Patterns)
		{
			return Out;
		}
		for (const auto& Pair : (*Patterns)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetArray(Arr) || !Arr) continue;
			TArray<FString> List;
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty()) List.Add(S);
			}
			if (List.Num() > 0)
			{
				// FJsonObject::Values keys are FStringType (UE::FSharedString in 5.8), not FString.
				Out.Add(FString(Pair.Key), MoveTemp(List));
			}
		}
		return Out;
	}

	bool NameMatchesPattern(const FString& AssetName, const FString& Pattern)
	{
		// Simple glob: *foo* / foo* / *foo / exact. Case-insensitive.
		FString Pat = Pattern;
		FString Name = AssetName;
		Pat.ToLowerInline();
		Name.ToLowerInline();
		if (!Pat.Contains(TEXT("*")))
		{
			return Name.Equals(Pat);
		}
		TArray<FString> Parts;
		Pat.ParseIntoArray(Parts, TEXT("*"), true);
		if (Parts.Num() == 0) return true;
		const bool bStartsWild = Pattern.StartsWith(TEXT("*"));
		const bool bEndsWild = Pattern.EndsWith(TEXT("*"));
		int32 Cursor = 0;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			const int32 Found = Name.Find(Parts[i], ESearchCase::IgnoreCase, ESearchDir::FromStart, Cursor);
			if (Found == INDEX_NONE) return false;
			if (i == 0 && !bStartsWild && Found != 0) return false;
			Cursor = Found + Parts[i].Len();
		}
		if (!bEndsWild && Cursor != Name.Len()) return false;
		return true;
	}

	bool ClassAllowed(const FAssetData& Asset, const TArray<FString>& ClassFilter)
	{
		if (ClassFilter.Num() == 0) return true;
		const FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();
		for (const FString& C : ClassFilter)
		{
			if (ClassName.Equals(C, ESearchCase::IgnoreCase)) return true;
		}
		return false;
	}
}

FString UUeremcpEnvironmentToolset::FindProjectAssets(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("find_project_assets"), Request, Rejection))
	{
		return Rejection;
	}

	TArray<FString> Roles;
	TArray<FString> ClassFilter;
	TArray<FString> PathScopes;
	int32 MaxPerRole = 5;
	if (Request.Specification.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* RoleArr = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("roles"), RoleArr) && RoleArr)
		{
			for (const TSharedPtr<FJsonValue>& V : *RoleArr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty()) Roles.Add(S);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* ClassArr = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("class_filter"), ClassArr) && ClassArr)
		{
			for (const TSharedPtr<FJsonValue>& V : *ClassArr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty()) ClassFilter.Add(S);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* ScopeArr = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("path_scopes"), ScopeArr) && ScopeArr)
		{
			for (const TSharedPtr<FJsonValue>& V : *ScopeArr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty()) PathScopes.Add(S);
			}
		}
		double Max = MaxPerRole;
		if (Request.Specification->TryGetNumberField(TEXT("max_per_role"), Max))
		{
			MaxPerRole = FMath::Clamp(static_cast<int32>(Max), 1, 50);
		}
	}
	if (Roles.Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("find_project_assets requires specification.roles: a non-empty array of "
				 "semantic roles such as foliage.tree, foliage.grass, structure.wall."));
	}
	if (ClassFilter.Num() == 0) ClassFilter.Add(TEXT("StaticMesh"));
	if (PathScopes.Num() == 0)
	{
		PathScopes.Add(TEXT("/Game"));
		PathScopes.Add(TEXT("/Engine/BasicShapes"));
	}

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = ARM.Get();

	// [VERIFIED: IAssetRegistry.h:1025] IsLoadingAssets
	if (Registry.IsLoadingAssets())
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("partially_completed");
		Response.Summary = TEXT(
			"AssetRegistry is still loading; results would read as an empty project. "
			"Retry find_project_assets after the initial scan finishes.");
		Response.ErrorCode = TEXT("ASSET_REGISTRY_LOADING");
		Response.CapabilityNotes.Add(
			TEXT("IsLoadingAssets() was true. An empty resolved[] here would be a silent lie."));
		Response.Metrics.McpRoundTrips = 1;
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("resolved"), {});
		Result->SetArrayField(TEXT("unresolved"), {});
		Response.ExtraFields = MakeShared<FJsonObject>();
		Response.ExtraFields->SetObjectField(TEXT("result"), Result);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	for (const FString& Scope : PathScopes)
	{
		Filter.PackagePaths.Add(FName(*Scope));
	}
	TArray<FAssetData> Found;
	Registry.GetAssets(Filter, Found);

	const TMap<FString, TArray<FString>> RolePatterns = LoadRolePatterns();
	TArray<TSharedPtr<FJsonValue>> Resolved;
	TArray<TSharedPtr<FJsonValue>> Unresolved;

	for (const FString& Role : Roles)
	{
		const TArray<FString>* Patterns = RolePatterns.Find(Role);
		TArray<FString> Searched = Patterns ? *Patterns : TArray<FString>{};
		if (!Patterns)
		{
			TSharedPtr<FJsonObject> Missing = MakeShared<FJsonObject>();
			Missing->SetStringField(TEXT("role"), Role);
			Missing->SetArrayField(TEXT("searched"), {});
			TSharedPtr<FJsonObject> Satisfied = MakeShared<FJsonObject>();
			Satisfied->SetStringField(
				TEXT("action"),
				TEXT("editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file"));
			Missing->SetObjectField(TEXT("satisfied_by"), Satisfied);
			Missing->SetStringField(
				TEXT("note"),
				TEXT("unknown role — add patterns under asset_role_patterns in operation_catalog.json"));
			Unresolved.Add(MakeShared<FJsonValueObject>(Missing));
			continue;
		}

		TArray<TSharedPtr<FJsonValue>> Matches;
		for (const FAssetData& Asset : Found)
		{
			if (!ClassAllowed(Asset, ClassFilter)) continue;
			const FString AssetName = Asset.AssetName.ToString();
			bool bHit = false;
			for (const FString& Pat : *Patterns)
			{
				if (NameMatchesPattern(AssetName, Pat))
				{
					bHit = true;
					break;
				}
			}
			if (!bHit) continue;

			TSharedPtr<FJsonObject> Match = MakeShared<FJsonObject>();
			Match->SetStringField(TEXT("path"), Asset.GetObjectPathString());
			Match->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
			bool bNanite = false;
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
			{
				bNanite = Mesh->GetNaniteSettings().bEnabled;
			}
			Match->SetBoolField(TEXT("nanite"), bNanite);
			Matches.Add(MakeShared<FJsonValueObject>(Match));
			if (Matches.Num() >= MaxPerRole) break;
		}

		if (Matches.Num() > 0)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("role"), Role);
			Entry->SetArrayField(TEXT("matches"), Matches);
			Resolved.Add(MakeShared<FJsonValueObject>(Entry));
		}
		else
		{
			TSharedPtr<FJsonObject> Missing = MakeShared<FJsonObject>();
			Missing->SetStringField(TEXT("role"), Role);
			TArray<TSharedPtr<FJsonValue>> SearchedJson;
			for (const FString& P : Searched)
			{
				SearchedJson.Add(MakeShared<FJsonValueString>(P));
			}
			Missing->SetArrayField(TEXT("searched"), SearchedJson);
			TSharedPtr<FJsonObject> Satisfied = MakeShared<FJsonObject>();
			Satisfied->SetStringField(
				TEXT("action"),
				TEXT("editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file"));
			Missing->SetObjectField(TEXT("satisfied_by"), Satisfied);
			Unresolved.Add(MakeShared<FJsonValueObject>(Missing));
		}
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("AssetRegistry probe: %d role(s) resolved, %d unresolved."),
		Resolved.Num(), Unresolved.Num());
	Response.Metrics.McpRoundTrips = 1;
	Response.CapabilityNotes.Add(
		TEXT("unresolved is a first-class outcome — never treat an empty matches list as a path."));
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("resolved"), Resolved);
	Result->SetArrayField(TEXT("unresolved"), Unresolved);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	return FUeremcpEnvelope::SerializeResponse(Response);
}
