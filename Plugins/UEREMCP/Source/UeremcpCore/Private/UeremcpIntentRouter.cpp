#include "UeremcpIntentRouter.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpCapabilityService.h"
#include "UeremcpSchemaPublishing.h"

namespace UeremcpIntentRouterInternal
{
	struct FToolDoc
	{
		FString Toolset;
		FString Tool;
		FString Qualified;
		FString Description;
		TArray<FString> Properties;
		TArray<FString> Required;
		TMap<FString, int32> Tokens;
		bool bIsUeremcp = false;
		FString SupersededBy;
		TSharedPtr<FJsonObject> CatalogOp;
	};

	struct FCatalog
	{
		TMap<FString, TSharedPtr<FJsonObject>> OpsByQualified;
		TMap<FString, TArray<FString>> DependsOn; // action -> prerequisites
		TMap<FString, FString> Why;
	};

	static const TSet<FString>& Stopwords()
	{
		static const TSet<FString> Words = {
			TEXT("a"), TEXT("an"), TEXT("the"), TEXT("and"), TEXT("or"), TEXT("of"), TEXT("to"),
			TEXT("for"), TEXT("in"), TEXT("on"), TEXT("with"), TEXT("from"), TEXT("by"), TEXT("at"),
			TEXT("is"), TEXT("are"), TEXT("be"), TEXT("it"), TEXT("its"), TEXT("this"), TEXT("that"),
			TEXT("me"), TEXT("my"), TEXT("i"), TEXT("we"), TEXT("you"), TEXT("your"), TEXT("new"),
			TEXT("make"), TEXT("get"), TEXT("set"), TEXT("use"), TEXT("using"), TEXT("want"),
			TEXT("need"), TEXT("some"), TEXT("kind"), TEXT("like"), TEXT("looks"), TEXT("look"),
			TEXT("what"), TEXT("which"), TEXT("how"), TEXT("do"), TEXT("does"), TEXT("can"),
			TEXT("should"), TEXT("would"), TEXT("one"), TEXT("all"), TEXT("any"), TEXT("into"),
			TEXT("out"), TEXT("up"), TEXT("down"), TEXT("when"),
			TEXT("open"), TEXT("world"), TEXT("game"), TEXT("feel"), TEXT("more"), TEXT("across"),
			TEXT("lay"), TEXT("build"), TEXT("system"), TEXT("screen"), TEXT("let"), TEXT("lets"),
			TEXT("players"), TEXT("join"), TEXT("display"), TEXT("temporary"), TEXT("import"),
			TEXT("layers"), TEXT("layer"), TEXT("ping"), TEXT("sessions"), TEXT("server"),
			TEXT("music"), TEXT("lobby")
		};
		return Words;
	}

	static TMap<FString, FString> Aliases()
	{
		// Evidence-led only — do not tune to baseline EVAL.
		return {
			{TEXT("spell"), TEXT("niagara effect vfx particle")},
			{TEXT("vfx"), TEXT("niagara effect particle")},
			{TEXT("particle"), TEXT("niagara effect emitter")},
			{TEXT("helix"), TEXT("niagara effect ribbon spiral")},
			{TEXT("beam"), TEXT("niagara effect emitter")},
			{TEXT("explosion"), TEXT("niagara effect burst")},
			{TEXT("shader"), TEXT("material")},
			{TEXT("texture"), TEXT("material texture")},
			{TEXT("free_spells"), TEXT("inspect material Free_Spells material graph")},
			{TEXT("m_free_spells"), TEXT("inspect material Free_Spells")},
			{TEXT("mi_free_spells"), TEXT("inspect material Free_Spells material instance")},
			{TEXT("screenshot"), TEXT("capture viewport image render")},
			{TEXT("picture"), TEXT("capture viewport image render")},
			{TEXT("look"), TEXT("capture viewport image render")},
			{TEXT("animation"), TEXT("montage sequence anim skeletal")},
			{TEXT("logic"), TEXT("blueprint graph node")},
			{TEXT("ability"), TEXT("gameplay ability gas")},
		};
	}

	static void AppendTokens(const FString& Text, TArray<FString>& Out)
	{
		FString Lower = Text.ToLower();
		FString Current;
		auto Flush = [&]()
		{
			if (Current.Len() > 1 && !Stopwords().Contains(Current))
			{
				Out.Add(Current);
			}
			Current.Reset();
		};
		for (int32 i = 0; i < Lower.Len(); ++i)
		{
			const TCHAR C = Lower[i];
			if (FChar::IsAlnum(C))
			{
				Current.AppendChar(C);
			}
			else
			{
				Flush();
			}
		}
		Flush();
	}

	static TArray<FString> Tokenize(const FString& Text)
	{
		TArray<FString> Out;
		AppendTokens(Text, Out);
		// Split PascalCase-ish runs already lowercased — keep simple: also split on digit boundaries skipped
		return Out;
	}

	static TArray<FString> ExpandQuery(const FString& Query)
	{
		TArray<FString> Tok = Tokenize(Query);
		const TMap<FString, FString> Map = Aliases();
		TArray<FString> Extra;
		for (const FString& T : Tok)
		{
			if (const FString* Exp = Map.Find(T))
			{
				Extra.Append(Tokenize(*Exp));
			}
		}
		Tok.Append(Extra);
		return Tok;
	}

	static FString NormalizeToolName(const FString& Name)
	{
		// Backward-compatible lookup alias: PascalCase, snake_case, kebab-case,
		// and case-only variants resolve to the live registry spelling. This
		// never registers a second callable or changes the canonical name.
		FString Out;
		Out.Reserve(Name.Len());
		for (const TCHAR C : Name)
		{
			if (FChar::IsAlnum(C))
			{
				Out.AppendChar(FChar::ToLower(C));
			}
		}
		return Out;
	}

	static FString PluginContentCatalogPath()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UEREMCP")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/IntentRouter/operation_catalog.json"));
		}
		return FString();
	}

	static bool LoadCatalog(FCatalog& Out, FString& Error)
	{
		const FString Path = PluginContentCatalogPath();
		FString Raw;
		if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Raw, *Path))
		{
			Error = TEXT("operation_catalog.json not found under plugin Content/IntentRouter");
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Error = TEXT("operation_catalog.json failed to parse");
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (Root->TryGetArrayField(TEXT("operations"), Ops) && Ops)
		{
			for (const TSharedPtr<FJsonValue>& V : *Ops)
			{
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) continue;
				FString Qualified;
				if (Obj->TryGetStringField(TEXT("qualified"), Qualified) && !Qualified.IsEmpty())
				{
					Out.OpsByQualified.Add(Qualified, Obj);
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Deps = nullptr;
		if (Root->TryGetArrayField(TEXT("dependencies"), Deps) && Deps)
		{
			for (const TSharedPtr<FJsonValue>& V : *Deps)
			{
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) continue;
				FString Action;
				if (!Obj->TryGetStringField(TEXT("action"), Action)) continue;
				TArray<FString> Pred;
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (Obj->TryGetArrayField(TEXT("depends_on_actions"), Arr) && Arr)
				{
					for (const TSharedPtr<FJsonValue>& D : *Arr)
					{
						Pred.Add(D->AsString());
					}
				}
				Out.DependsOn.Add(Action, Pred);
				FString Why;
				if (Obj->TryGetStringField(TEXT("why"), Why))
				{
					Out.Why.Add(Action, Why);
				}
			}
		}
		return true;
	}

	static bool ParseLiveSchemas(TArray<FToolDoc>& OutDocs, TSet<FString>& OutNames, FString& Error)
	{
		if (!UToolsetRegistry::IsAvailable())
		{
			Error = TEXT("ToolsetRegistry unavailable");
			return false;
		}
		const FString SchemasJson = UToolsetRegistry::GetAllToolsetJsonSchemas();
		TArray<TSharedPtr<FJsonValue>> Arr;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SchemasJson);
		if (!FJsonSerializer::Deserialize(Reader, Arr))
		{
			// Some builds return a single object or {"toolsets":[...]}
			TSharedPtr<FJsonValue> RootVal;
			const TSharedRef<TJsonReader<>> Reader2 = TJsonReaderFactory<>::Create(SchemasJson);
			if (FJsonSerializer::Deserialize(Reader2, RootVal) && RootVal.IsValid())
			{
				if (RootVal->Type == EJson::Array)
				{
					Arr = RootVal->AsArray();
				}
				else if (RootVal->Type == EJson::Object)
				{
					const TSharedPtr<FJsonObject> Obj = RootVal->AsObject();
					const TArray<TSharedPtr<FJsonValue>>* Nested = nullptr;
					if (Obj->TryGetArrayField(TEXT("toolsets"), Nested) && Nested)
					{
						Arr = *Nested;
					}
					else
					{
						Arr.Add(RootVal);
					}
				}
			}
		}
		if (Arr.Num() == 0)
		{
			Error = TEXT("GetAllToolsetJsonSchemas returned no toolsets");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Entry : Arr)
		{
			const TSharedPtr<FJsonObject> Ts = Entry->AsObject();
			if (!Ts.IsValid()) continue;
			FString TsName;
			Ts->TryGetStringField(TEXT("name"), TsName);
			if (TsName.IsEmpty()) continue;
			FString TsDesc;
			Ts->TryGetStringField(TEXT("description"), TsDesc);
			const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
			if (!Ts->TryGetArrayField(TEXT("tools"), Tools) || !Tools) continue;
			for (const TSharedPtr<FJsonValue>& TV : *Tools)
			{
				const TSharedPtr<FJsonObject> Tool = TV->AsObject();
				if (!Tool.IsValid()) continue;
				FString FullName;
				Tool->TryGetStringField(TEXT("name"), FullName);
				FString Short = FullName;
				int32 Dot = INDEX_NONE;
				if (FullName.FindLastChar(TEXT('.'), Dot))
				{
					Short = FullName.RightChop(Dot + 1);
				}
				const FString Qualified = TsName + TEXT(".") + Short;
				FToolDoc Doc;
				Doc.Toolset = TsName;
				Doc.Tool = Short;
				Doc.Qualified = Qualified;
				Tool->TryGetStringField(TEXT("description"), Doc.Description);
				Doc.bIsUeremcp = TsName.StartsWith(TEXT("Ueremcp"));
				const TSharedPtr<FJsonObject>* InputSchema = nullptr;
				if (Tool->TryGetObjectField(TEXT("inputSchema"), InputSchema) && InputSchema && InputSchema->IsValid())
				{
					const TSharedPtr<FJsonObject>* Props = nullptr;
					if ((*InputSchema)->TryGetObjectField(TEXT("properties"), Props) && Props)
					{
						for (const auto& Pair : (*Props)->Values)
						{
							Doc.Properties.Add(FString(Pair.Key));
						}
					}
					const TArray<TSharedPtr<FJsonValue>>* Req = nullptr;
					if ((*InputSchema)->TryGetArrayField(TEXT("required"), Req) && Req)
					{
						for (const TSharedPtr<FJsonValue>& R : *Req)
						{
							Doc.Required.Add(R->AsString());
						}
					}
				}
				TArray<FString> BlobTokens;
				AppendTokens(TsName, BlobTokens);
				AppendTokens(Short, BlobTokens);
				AppendTokens(Doc.Description, BlobTokens);
				AppendTokens(TsDesc, BlobTokens);
				for (const FString& P : Doc.Properties)
				{
					AppendTokens(P, BlobTokens);
				}
				for (const FString& T : BlobTokens)
				{
					Doc.Tokens.FindOrAdd(T)++;
				}
				OutNames.Add(Qualified);
				OutDocs.Add(MoveTemp(Doc));
			}
		}
		return OutDocs.Num() > 0;
	}

	static void ApplyCatalogEnrichment(TArray<FToolDoc>& Docs, const FCatalog& Catalog)
	{
		// SUPERSEDED demotion map (same patterns as tools/gen_focus_config.py — keep in sync).
		static const TArray<TPair<FString, FString>> Superseded = {
			{TEXT("NiagaraToolsets.NiagaraToolset_System"), TEXT("UeremcpNiagara.UeremcpNiagaraToolset")},
			{TEXT("editor_toolset.toolsets.material.MaterialTools"), TEXT("UeremcpMaterial.UeremcpMaterialToolset")},
			{TEXT("editor_toolset.toolsets.blueprint.BlueprintTools"), TEXT("UeremcpBlueprint.UeremcpBlueprintToolset")},
			{TEXT("re_agent_tools.toolsets.niagara_workflow_tools"), TEXT("UeremcpNiagara.UeremcpNiagaraToolset")},
			{TEXT("re_agent_tools.toolsets.material_workflow_tools"), TEXT("UeremcpMaterial.UeremcpMaterialToolset")},
			{TEXT("re_agent_tools.toolsets.blueprint_workflow_tools"), TEXT("UeremcpBlueprint.UeremcpBlueprintToolset")},
			{TEXT("re_agent_tools.toolsets.anim_workflow_tools"), TEXT("UeremcpAnimation.UeremcpAnimationToolset")},
		};

		for (FToolDoc& Doc : Docs)
		{
			if (const TSharedPtr<FJsonObject>* Op = Catalog.OpsByQualified.Find(Doc.Qualified))
			{
				Doc.CatalogOp = *Op;
				const TArray<TSharedPtr<FJsonValue>>* UseWhen = nullptr;
				if ((*Op)->TryGetArrayField(TEXT("use_when"), UseWhen) && UseWhen)
				{
					for (const TSharedPtr<FJsonValue>& V : *UseWhen)
					{
						for (const FString& T : Tokenize(V->AsString()))
						{
							Doc.Tokens.FindOrAdd(T)++;
						}
					}
				}
			}
			for (const TPair<FString, FString>& Pair : Superseded)
			{
				if (Doc.Toolset.Contains(Pair.Key) || Doc.Toolset.Equals(Pair.Key, ESearchCase::IgnoreCase))
				{
					Doc.SupersededBy = Pair.Value;
					break;
				}
			}
		}
	}

	static double ScoreDoc(const TArray<FString>& QueryToks, const FToolDoc& Doc, int32 N,
		const TMap<FString, int32>& Df, double AvgDl)
	{
		const double K1 = 1.5;
		const double B = 0.75;
		int32 Dl = 0;
		for (const auto& Pair : Doc.Tokens)
		{
			Dl += Pair.Value;
		}
		Dl = FMath::Max(Dl, 1);
		double Score = 0.0;
		for (const FString& T : QueryToks)
		{
			const int32* TfPtr = Doc.Tokens.Find(T);
			if (!TfPtr) continue;
			const int32 DfT = Df.FindRef(T);
			const double Idf = FMath::Loge(1.0 + (N - DfT + 0.5) / (DfT + 0.5));
			const double Tf = static_cast<double>(*TfPtr);
			Score += Idf * (Tf * (K1 + 1.0)) / (Tf + K1 * (1.0 - B + B * Dl / AvgDl));
		}
		if (Score <= 0.0) return 0.0;
		if (Doc.bIsUeremcp) Score *= 1.6;
		if (!Doc.SupersededBy.IsEmpty()) Score *= 0.35;
		return Score;
	}

	static int32 ActionRank(const FString& Action, const FCatalog& Catalog)
	{
		if (Action.IsEmpty()) return 50;
		const TArray<FString>* Pred = Catalog.DependsOn.Find(Action);
		if (!Pred || Pred->Num() == 0) return 10;
		int32 MaxP = 10;
		for (const FString& P : *Pred)
		{
			MaxP = FMath::Max(MaxP, ActionRank(P, Catalog) + 1);
		}
		return MaxP;
	}

	// Digest of sorted qualified names — same algorithm as tools/intent_router/router.py
	// (SHA-1 over UTF-8 joined by newlines). [VERIFIED: Misc/SecureHash.h FSHA1]
	static FString Sha256Names(const TSet<FString>& Names)
	{
		TArray<FString> Sorted = Names.Array();
		Sorted.Sort();
		const FString Blob = FString::Join(Sorted, TEXT("\n"));
		FTCHARToUTF8 Utf8(*Blob);
		FSHAHash Hash;
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
		return Hash.ToString().ToLower();
	}
} // namespace

FString FUeremcpIntentRouter::ComputeLiveRegistryHash()
{
	return FUeremcpCapabilityService::ComputeLiveRegistryHash();
}

bool FUeremcpIntentRouter::LiveRegistryContains(const FString& QualifiedToolName)
{
	using namespace UeremcpIntentRouterInternal;
	TArray<FToolDoc> Docs;
	TSet<FString> Names;
	FString Error;
	if (!ParseLiveSchemas(Docs, Names, Error))
	{
		return false;
	}
	return Names.Contains(QualifiedToolName);
}

FUeremcpIntentRouterResult FUeremcpIntentRouter::GetStarted(const FString& Detail)
{
	FUeremcpIntentRouterResult Result;
	Result.Status = TEXT("no_change_required");
	Result.Summary = TEXT("START HERE: call ResolveIntent with your plain-text goal. Prefer UEREMCP semantic tools over Epic primitives.");
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("start_here"), TEXT("UeremcpCore.UeremcpReferenceToolset.ResolveIntent"));
	Payload->SetStringField(TEXT("also"), TEXT("DescribeOperation for one tool; Ping for reachability"));
	Payload->SetStringField(TEXT("policy"), TEXT("Prefer Ueremcp* goal-level tools; use Epic for read-only discovery gaps only"));
	Payload->SetStringField(TEXT("envelope"),
		TEXT("protocol_version + action + specification; domain tools use options.dry_run, while ExecutePreparedAction uses top-level dry_run"));
	Payload->SetStringField(TEXT("next_call"),
		TEXT("ResolveIntent with specification.intent = your goal, mode=recommend"));
	TArray<TSharedPtr<FJsonValue>> Prefer;
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpEnvironment.UeremcpEnvironmentToolset")));
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpUI.UeremcpUIToolset")));
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpNiagara.UeremcpNiagaraToolset")));
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpMaterial.UeremcpMaterialToolset")));
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpBlueprint.UeremcpBlueprintToolset")));
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpTemplates.UeremcpTemplatesToolset")));
	Prefer.Add(MakeShared<FJsonValueString>(TEXT("UeremcpValidation.UeremcpVisualCaptureToolset")));
	Payload->SetArrayField(TEXT("prefer_toolsets"), Prefer);
	Payload->SetStringField(TEXT("niagara_path_policy"),
		TEXT("YES you can create NEW multi-emitter effects under Magecraft+sandbox. "
			 "READ: UeremcpNiagara.InspectSystem for any /Game/… including Magecraft — one call returns result.graphs[]. "
			 "WRITE new (ONE SHOT): CreateNiagaraEffect with specification.components[] or emitters[{role|template_path,name}] "
			 "under /Game/__UeremcpTests|/Game/__UeremcpPoc|/Game/RE/VFX/Magecraft — adds real emitters (not empty shell). "
			 "Example ice: emitters ice_creep+freeze_dome+sparks. "
			 "WRITE light tweaks: AdaptNiagaraEffect (User.*/materials, never deletes). "
			 "WRITE structural edits: InspectSystem → SubmitNiagaraGraph (add modules / add emitters from template_path). "
			 "mode=replace delete is sandbox-only — Magecraft replace is in-place stack reconcile. "
			 "round_trip_supported=false means hash not proven — NOT that authoring is disabled. "
			 "CaptureEffectFrames for visual proof."));
	Payload->SetStringField(TEXT("detail"), Detail.IsEmpty() ? TEXT("summary") : Detail);
	Payload->SetStringField(TEXT("set_name_filters_note"),
		TEXT("FToolset::SetNameFilters exists [VERIFIED: Toolset.h:59-60] but UEREMCP does not apply global hides; router demotes SUPERSEDED instead."));
	Payload->SetStringField(TEXT("ui_domain"),
		TEXT("UeremcpUI.UeremcpUIToolset — create_widget_from_spec, show_widget_in_world (preferred MMO overlay / CaptureViewport path), spawn_character_preview, create_inventory_sheet_from_spec. Screen UMG is invisible to CaptureViewport; use world-space hosts."));

	// MCP-013 / MCP-014 are external. Watch was fixed (UnrealWatchMCP v0.4.0) —
	// do NOT advertise do_not_use / available:false. Point agents at check_unreal.
	TArray<TSharedPtr<FJsonValue>> External;
	{
		TSharedPtr<FJsonObject> Watch = MakeShared<FJsonObject>();
		Watch->SetStringField(TEXT("name"), TEXT("user-unreal-watch"));
		Watch->SetBoolField(TEXT("available"), true);
		Watch->SetStringField(TEXT("primary"), TEXT("check_unreal"));
		Watch->SetStringField(
			TEXT("tools"),
			TEXT("check_unreal,get_editor_status,wait_for_editor,dismiss_dialog,get_watch_config,set_watch_config"));
		Watch->SetStringField(
			TEXT("use_when"),
			TEXT("Before Unreal MCP timeout / 10061 / empty-upstream retry loops; "
				 "confirm editor_offline vs ok vs modal_blocked. Prefer check_unreal; "
				 "use get_editor_status for a lighter poll."));
		Watch->SetStringField(
			TEXT("fallback"),
			TEXT("If watch MCP is not discovered: python UnrealWatchMCP/server.py --check"));
		External.Add(MakeShared<FJsonValueObject>(Watch));
		TSharedPtr<FJsonObject> Sem = MakeShared<FJsonObject>();
		Sem->SetStringField(TEXT("name"), TEXT("SemanticSearch"));
		Sem->SetBoolField(TEXT("available"), false);
		Sem->SetStringField(TEXT("fallback"), TEXT("Use ResolveIntent + find_project_assets; do not call SemanticSearch."));
		External.Add(MakeShared<FJsonValueObject>(Sem));
	}
	Payload->SetArrayField(TEXT("external_mcp_capabilities"), External);
	Payload->SetStringField(TEXT("asset_probe"),
		TEXT("UeremcpEnvironment.UeremcpEnvironmentToolset.FindProjectAssets — ask the AssetRegistry before inventing mesh paths."));
	Payload->SetStringField(
		TEXT("capture_beauty"),
		TEXT("Prefer editor viewport / CaptureMaterialFrames for beauty review — not a structural gate."));
	Payload->SetStringField(
		TEXT("material_path_policy"),
		TEXT("READ any /Game material/MI (incl. Free_Spells): InspectMaterial → result.graphs[] + parameters. "
			 "WRITE existing: SubmitMaterialGraph (in-place; never silent-delete production masters). "
			 "WRITE new VFX: CreateVfxMaterial / CreateMasterMaterial under scratch. "
			 "fidelity.round_trip_supported=false until proven. Proof: CaptureMaterialFrames."));
	Payload->SetStringField(
		TEXT("capture_structural"),
		TEXT("UeremcpValidation.UeremcpVisualCaptureToolset.CaptureWorldFrames — structural world evidence after env builds."));
	Result.Payload = Payload;
	return Result;
}

FUeremcpIntentRouterResult FUeremcpIntentRouter::DescribeOperation(const TSharedPtr<FJsonObject>& Spec)
{
	using namespace UeremcpIntentRouterInternal;
	FUeremcpIntentRouterResult Result;

	FString ToolQuery;
	FString Detail = TEXT("slim");
	FString IfNoneMatch;
	if (Spec.IsValid())
	{
		Spec->TryGetStringField(TEXT("tool"), ToolQuery);
		Spec->TryGetStringField(TEXT("detail"), Detail);
		Spec->TryGetStringField(TEXT("if_none_match"), IfNoneMatch);
	}
	if (ToolQuery.IsEmpty())
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("specification.tool is required");
		return Result;
	}
	if (!Detail.Equals(TEXT("index"), ESearchCase::IgnoreCase)
		&& !Detail.Equals(TEXT("slim"), ESearchCase::IgnoreCase)
		&& !Detail.Equals(TEXT("full"), ESearchCase::IgnoreCase))
	{
		Detail = TEXT("slim");
	}

	TArray<FToolDoc> Docs;
	TSet<FString> Names;
	FString Error;
	if (!ParseLiveSchemas(Docs, Names, Error))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = Error;
		return Result;
	}
	FCatalog Catalog;
	FString CatalogError;
	LoadCatalog(Catalog, CatalogError);
	ApplyCatalogEnrichment(Docs, Catalog);

	const FToolDoc* Found = nullptr;
	TArray<const FToolDoc*> NormalizedMatches;
	const FString NormalizedQuery = NormalizeToolName(ToolQuery);
	for (const FToolDoc& Doc : Docs)
	{
		if (Doc.Qualified.Equals(ToolQuery, ESearchCase::IgnoreCase)
			|| Doc.Tool.Equals(ToolQuery, ESearchCase::IgnoreCase))
		{
			Found = &Doc;
			if (Doc.Qualified.Equals(ToolQuery, ESearchCase::IgnoreCase))
			{
				break;
			}
		}
		else if (NormalizeToolName(Doc.Qualified).Equals(NormalizedQuery)
			|| NormalizeToolName(Doc.Tool).Equals(NormalizedQuery))
		{
			NormalizedMatches.Add(&Doc);
		}
	}
	if (!Found && NormalizedMatches.Num() == 1)
	{
		Found = NormalizedMatches[0];
	}
	else if (!Found && NormalizedMatches.Num() > 1)
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(
			TEXT("Tool alias '%s' is ambiguous; use a fully-qualified live registry name"),
			*ToolQuery);
		for (const FToolDoc* Match : NormalizedMatches)
		{
			Result.CapabilityNotes.Add(Match->Qualified);
		}
		return Result;
	}
	if (!Found)
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(TEXT("Tool '%s' is not in the live registry"), *ToolQuery);
		Result.CapabilityNotes.Add(TEXT("Call ResolveIntent or list_toolsets; never invent tool names."));
		return Result;
	}

	auto OneLine = [](const FString& Text) -> FString
	{
		FString Out = Text;
		int32 Nl = INDEX_NONE;
		if (Out.FindChar(TEXT('\n'), Nl))
		{
			Out = Out.Left(Nl).TrimEnd();
		}
		if (Out.Len() > 160) Out = Out.Left(157) + TEXT("...");
		return Out;
	};

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("toolset"), Found->Toolset);
	Payload->SetStringField(TEXT("tool"), Found->Tool);
	Payload->SetStringField(TEXT("qualified"), Found->Qualified);
	Payload->SetStringField(TEXT("detail"), Detail.ToLower());
	if (!ToolQuery.Equals(Found->Tool, ESearchCase::CaseSensitive)
		&& !ToolQuery.Equals(Found->Qualified, ESearchCase::CaseSensitive))
	{
		Payload->SetStringField(TEXT("normalized_from"), ToolQuery);
	}

	if (Detail.Equals(TEXT("index"), ESearchCase::IgnoreCase))
	{
		Payload->SetStringField(TEXT("description"), OneLine(Found->Description));
		if (Found->CatalogOp.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* UseWhen = nullptr;
			if (Found->CatalogOp->TryGetArrayField(TEXT("use_when"), UseWhen) && UseWhen)
			{
				Payload->SetArrayField(TEXT("use_when"), *UseWhen);
			}
		}
	}
	else if (Detail.Equals(TEXT("slim"), ESearchCase::IgnoreCase))
	{
		Payload->SetStringField(TEXT("description"), OneLine(Found->Description));
		if (Found->CatalogOp.IsValid())
		{
			static const TCHAR* SlimFields[] = {
				TEXT("use_when"), TEXT("do_not_use_for"), TEXT("expected_statuses"),
				TEXT("recovery"), TEXT("batch_hint")
			};
			for (const TCHAR* Field : SlimFields)
			{
				if (Found->CatalogOp->HasField(Field))
				{
					Payload->SetField(Field, Found->CatalogOp->TryGetField(Field));
				}
			}
			const TSharedPtr<FJsonObject>* Example = nullptr;
			if (Found->CatalogOp->TryGetObjectField(TEXT("example_request"), Example) && Example)
			{
				Payload->SetObjectField(TEXT("request_json"), *Example);
			}
			if (Found->CatalogOp->HasField(TEXT("destructive")))
			{
				Payload->SetField(TEXT("destructive"), Found->CatalogOp->TryGetField(TEXT("destructive")));
			}
		}
		// Required fields from UHT, not the 83 KB nested mirror.
		TArray<TSharedPtr<FJsonValue>> Req;
		for (const FString& R : Found->Required)
		{
			Req.Add(MakeShared<FJsonValueString>(R));
		}
		Payload->SetArrayField(TEXT("required"), Req);
	}
	else // full
	{
		Payload->SetStringField(TEXT("description"), Found->Description);
		if (const TSharedPtr<FJsonObject> Nested =
				UeremcpSchemaPublishing::BuildNestedRequestSchemaForTool(Found->Tool))
		{
			Payload->SetObjectField(TEXT("input_schema"), Nested);
		}
		else
		{
			TArray<TSharedPtr<FJsonValue>> Props;
			for (const FString& P : Found->Properties)
			{
				Props.Add(MakeShared<FJsonValueString>(P));
			}
			TArray<TSharedPtr<FJsonValue>> Req;
			for (const FString& R : Found->Required)
			{
				Req.Add(MakeShared<FJsonValueString>(R));
			}
			TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
			Schema->SetArrayField(TEXT("properties"), Props);
			Schema->SetArrayField(TEXT("required"), Req);
			Payload->SetObjectField(TEXT("input_schema"), Schema);
		}
		if (Found->CatalogOp.IsValid())
		{
			const TSharedPtr<FJsonObject>* Example = nullptr;
			if (Found->CatalogOp->TryGetObjectField(TEXT("example_request"), Example) && Example)
			{
				Payload->SetObjectField(TEXT("request_json"), *Example);
			}
			// Short catalog fields only — not the whole op dump (avoids duplicating
			// the nested envelope mirror that bloated Environment describes to ~83 KB).
			TSharedPtr<FJsonObject> SlimCatalog = MakeShared<FJsonObject>();
			static const TCHAR* CatalogFields[] = {
				TEXT("action"), TEXT("use_when"), TEXT("do_not_use_for"),
				TEXT("expected_statuses"), TEXT("recovery"), TEXT("batch_hint"),
				TEXT("destructive"), TEXT("idempotent")
			};
			for (const TCHAR* Field : CatalogFields)
			{
				if (Found->CatalogOp->HasField(Field))
				{
					SlimCatalog->SetField(Field, Found->CatalogOp->TryGetField(Field));
				}
			}
			Payload->SetObjectField(TEXT("catalog"), SlimCatalog);
		}
	}
	if (!Found->SupersededBy.IsEmpty())
	{
		Payload->SetStringField(TEXT("warning"),
			FString::Printf(TEXT("superseded; prefer %s"), *Found->SupersededBy));
	}

	// content_hash for if_none_match caching (MCP-007).
	FString PayloadJson;
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
		FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
	}
	const FTCHARToUTF8 Utf8(*PayloadJson);
	const FString ContentHash = FMD5::HashBytes(
		reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	Payload->SetStringField(TEXT("content_hash"), ContentHash);

	if (!IfNoneMatch.IsEmpty() && IfNoneMatch.Equals(ContentHash, ESearchCase::IgnoreCase))
	{
		Result.Status = TEXT("no_change_required");
		Result.Summary = TEXT("Describe unchanged (if_none_match matched content_hash).");
		TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
		Empty->SetStringField(TEXT("content_hash"), ContentHash);
		Empty->SetBoolField(TEXT("not_modified"), true);
		Result.Payload = Empty;
		return Result;
	}

	Result.Status = TEXT("no_change_required");
	Result.Summary = FString::Printf(TEXT("Described live tool %s (%s)"), *Found->Qualified, *Detail.ToLower());
	Result.Payload = Payload;
	return Result;
}

FUeremcpIntentRouterResult FUeremcpIntentRouter::ResolveIntent(
	const FString& Intent,
	const FString& Mode,
	const TSharedPtr<FJsonObject>& Context,
	const FString& ExpectedRegistryHash,
	int32 MaxSteps)
{
	using namespace UeremcpIntentRouterInternal;
	FUeremcpIntentRouterResult Result;
	TArray<FToolDoc> Docs;
	TSet<FString> Names;
	FString Error;
	if (!ParseLiveSchemas(Docs, Names, Error))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = Error;
		return Result;
	}
	FCatalog Catalog;
	FString CatalogError;
	if (!LoadCatalog(Catalog, CatalogError))
	{
		Result.CapabilityNotes.Add(CatalogError);
	}
	ApplyCatalogEnrichment(Docs, Catalog);

	const FString LiveHash = FUeremcpCapabilityService::ComputeLiveRegistryHash();
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("intent"), Intent);
	Payload->SetStringField(TEXT("mode"), Mode.IsEmpty() ? TEXT("recommend") : Mode);
	Payload->SetStringField(TEXT("registry_hash"), LiveHash);
	Payload->SetStringField(TEXT("envelope_contract"),
		TEXT("UEREMCP tools take ONE string arg requestJson. Domain tool envelopes use options.dry_run; ExecutePreparedAction accepts canonical top-level dry_run."));

	if (!ExpectedRegistryHash.IsEmpty() && !ExpectedRegistryHash.Equals(LiveHash, ESearchCase::IgnoreCase))
	{
		Payload->SetStringField(TEXT("confidence"), TEXT("none"));
		Payload->SetStringField(TEXT("confidence_reason"),
			TEXT("registry hash mismatch; refusing confident routing on a stale view"));
		Payload->SetBoolField(TEXT("abstained"), true);
		Payload->SetArrayField(TEXT("plan"), {});
		Payload->SetStringField(TEXT("fallback"), TEXT("Refresh live registry view and call ResolveIntent again"));
		Result.Status = TEXT("no_change_required");
		Result.Summary = TEXT("Abstained: registry hash mismatch");
		Result.Payload = Payload;
		return Result;
	}

	if (Mode.Equals(TEXT("execute_if_complete"), ESearchCase::IgnoreCase))
	{
		TSharedPtr<FJsonObject> Exec = MakeShared<FJsonObject>();
		Exec->SetBoolField(TEXT("attempted"), false);
		Exec->SetStringField(TEXT("reason"),
			TEXT("execute_if_complete not enabled: cannot safely verify complete fields from plain text alone; use recommend + domain tools / InstantiateTemplate / ExecutePlan explicitly."));
		Payload->SetObjectField(TEXT("execution"), Exec);
	}

	const TArray<FString> QueryToks = ExpandQuery(Intent);
	const int32 N = Docs.Num();
	TMap<FString, int32> Df;
	double SumDl = 0.0;
	for (const FToolDoc& Doc : Docs)
	{
		int32 Dl = 0;
		for (const auto& Pair : Doc.Tokens)
		{
			Dl += Pair.Value;
		}
		SumDl += Dl;
		TSet<FString> Seen;
		for (const FString& T : QueryToks)
		{
			if (Doc.Tokens.Contains(T) && !Seen.Contains(T))
			{
				Df.FindOrAdd(T)++;
				Seen.Add(T);
			}
		}
	}
	const double AvgDl = SumDl / FMath::Max(N, 1);

	struct FHit
	{
		double Score = 0.0;
		TArray<FString> Matched;
		int32 DocIndex = INDEX_NONE;
		int32 OrderRank = 50;
		FString Why;
	};
	TArray<FHit> Hits;
	const TSet<FString> QuerySet(QueryToks);
	const bool bAllowProbeTools = QuerySet.Contains(TEXT("ping")) || QuerySet.Contains(TEXT("echo"))
		|| QuerySet.Contains(TEXT("liveness")) || QuerySet.Contains(TEXT("reachability"))
		|| QuerySet.Contains(TEXT("alive")) || QuerySet.Contains(TEXT("probe"));
	for (int32 i = 0; i < Docs.Num(); ++i)
	{
		if ((Docs[i].Tool.Equals(TEXT("Ping")) || Docs[i].Tool.Equals(TEXT("Echo"))
			|| Docs[i].Tool.Equals(TEXT("GetStarted")) || Docs[i].Tool.Equals(TEXT("ResolveIntent"))
			|| Docs[i].Tool.Equals(TEXT("DescribeOperation"))) && !bAllowProbeTools)
		{
			continue;
		}
		const double S = ScoreDoc(QueryToks, Docs[i], N, Df, AvgDl);
		if (S <= 0.0) continue;
		FHit H;
		H.Score = S;
		H.DocIndex = i;
		for (const FString& T : QueryToks)
		{
			if (Docs[i].Tokens.Contains(T))
			{
				H.Matched.AddUnique(T);
			}
		}
		FString Action;
		if (Docs[i].CatalogOp.IsValid())
		{
			Docs[i].CatalogOp->TryGetStringField(TEXT("action"), Action);
		}
		H.OrderRank = ActionRank(Action, Catalog);
		if (const FString* Why = Catalog.Why.Find(Action))
		{
			H.Why = *Why;
		}
		else
		{
			H.Why = TEXT("lexical match against live registry");
		}

		// BACKLOG 1b.3: when existing_assets are supplied, prefer inspect/capture/modify.
		if (Context.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
			if (Context->TryGetArrayField(TEXT("existing_assets"), Existing) && Existing && Existing->Num() > 0)
			{
				const FString ToolLower = Docs[i].Tool.ToLower();
				if (ToolLower.Contains(TEXT("inspect"))
					|| ToolLower.Contains(TEXT("capture"))
					|| ToolLower.Contains(TEXT("read"))
					|| ToolLower.Contains(TEXT("validate"))
					|| ToolLower.Contains(TEXT("modify"))
					|| ToolLower.Contains(TEXT("submit")))
				{
					H.Score *= 1.35;
					H.Why = TEXT("existing_assets present — prefer inspect/capture/modify over create");
				}
				else if (ToolLower.Contains(TEXT("create")) || ToolLower.Contains(TEXT("build")))
				{
					H.Score *= 0.75;
				}
			}
			FString DomainFilter;
			if (Context->TryGetStringField(TEXT("domain"), DomainFilter) && !DomainFilter.IsEmpty())
			{
				const FString TsLower = Docs[i].Toolset.ToLower();
				const FString DomLower = DomainFilter.ToLower();
				if (TsLower.Contains(DomLower)
					|| (DomLower.Equals(TEXT("capture")) && TsLower.Contains(TEXT("validation")))
					|| (DomLower.Equals(TEXT("material")) && TsLower.Contains(TEXT("material"))))
				{
					H.Score *= 1.25;
					H.Matched.AddUnique(TEXT("domain_filter"));
				}
				else if (Docs[i].bIsUeremcp)
				{
					H.Score *= 0.55;
				}
			}
		}
		Hits.Add(H);
	}

	// Material / Free_Spells intents must not misfire onto Niagara InspectSystem.
	{
		const FString IntentJoined = FString::Join(QueryToks, TEXT(" "));
		const bool bHasMaterialCue =
			IntentJoined.Contains(TEXT("material"))
			|| IntentJoined.Contains(TEXT("shader"))
			|| IntentJoined.Contains(TEXT("free_spells"))
			|| IntentJoined.Contains(TEXT("m_free"))
			|| IntentJoined.Contains(TEXT("mi_free"))
			|| (IntentJoined.Contains(TEXT("free")) && IntentJoined.Contains(TEXT("spells")));
		const bool bHasNiagaraCue =
			IntentJoined.Contains(TEXT("niagara"))
			|| IntentJoined.Contains(TEXT("emitter"))
			|| IntentJoined.Contains(TEXT("particle"))
			|| IntentJoined.Contains(TEXT("ns_"));
		if (bHasMaterialCue && !bHasNiagaraCue)
		{
			for (FHit& H : Hits)
			{
				FString Action;
				if (Docs.IsValidIndex(H.DocIndex) && Docs[H.DocIndex].CatalogOp.IsValid())
				{
					Docs[H.DocIndex].CatalogOp->TryGetStringField(TEXT("action"), Action);
				}
				const FString ToolLower = Docs.IsValidIndex(H.DocIndex)
					? Docs[H.DocIndex].Tool.ToLower()
					: FString();
				if (Action.Equals(TEXT("inspect_system"))
					|| ToolLower.Contains(TEXT("inspectsystem"))
					|| (ToolLower.Contains(TEXT("niagara")) && ToolLower.Contains(TEXT("inspect"))))
				{
					H.Score *= 0.2;
					H.Why = TEXT("demoted — material/Free_Spells intent; prefer InspectMaterial");
				}
				else if (Action.Equals(TEXT("inspect_material"))
					|| ToolLower.Contains(TEXT("inspectmaterial")))
				{
					H.Score *= 1.45;
					H.Why = TEXT("boosted — material/Free_Spells inspect intent");
					H.Matched.AddUnique(TEXT("material_intent"));
				}
			}
		}
	}

	Hits.Sort([](const FHit& A, const FHit& B) { return A.Score > B.Score; });

	// Dedupe by ACTION when the catalog knows one, else by toolset.
	//
	// Keying on toolset alone is why multi-domain goals collapsed to a single
	// step: every environment operation -- CreateLandscape, CreateWaterBody,
	// SubmitMeshOps, ScatterFoliage, AttachWeather -- lives in
	// UeremcpEnvironment.UeremcpEnvironmentToolset, so a five-operation build
	// kept exactly one candidate. Measured cold-start: an agent asking for a
	// forest on a hillside got back ScatterFoliage alone, pointing at a mesh
	// path that does not exist in an empty project.
	//
	// The toolset fallback still suppresses five near-identical uncatalogued
	// primitives, which is what the original key was protecting against.
	TMap<FString, FHit> BestPerKey;
	for (const FHit& H : Hits)
	{
		const FToolDoc& D = Docs[H.DocIndex];
		FString Key;
		if (D.CatalogOp.IsValid())
		{
			D.CatalogOp->TryGetStringField(TEXT("action"), Key);
		}
		if (Key.IsEmpty())
		{
			Key = D.Toolset;
		}
		if (!BestPerKey.Contains(Key) || BestPerKey[Key].Score < H.Score)
		{
			BestPerKey.Add(Key, H);
		}
	}
	TArray<FHit> Chosen;
	BestPerKey.GenerateValueArray(Chosen);
	// Score-primary ordering: dependsOn ranks only break near-ties. Declared domain
	// order used to force weakly matched toolsets into the plan (BACKLOG 1.3d).
	Chosen.Sort([](const FHit& A, const FHit& B)
	{
		const double Diff = FMath::Abs(A.Score - B.Score);
		if (Diff > 1.0) return A.Score > B.Score;
		if (A.OrderRank != B.OrderRank) return A.OrderRank < B.OrderRank;
		return A.Score > B.Score;
	});

	double BestHitScore = 0.0;
	for (const FHit& H : Hits)
	{
		BestHitScore = FMath::Max(BestHitScore, H.Score);
	}
	// Drop plan steps far below the best hit (BACKLOG 1.3d score-gate).
	constexpr double PlanScoreRatio = 0.35;
	const double PlanScoreFloor = BestHitScore * PlanScoreRatio;

	double UeremcpTop = 0.0;
	double CatalogTop = 0.0;
	TSet<FString> TopMatched;
	for (int32 i = 0; i < Chosen.Num() && i < 3; ++i)
	{
		if (Chosen[i].Score < PlanScoreFloor) continue;
		const FToolDoc& Doc = Docs[Chosen[i].DocIndex];
		if (Doc.bIsUeremcp) UeremcpTop = FMath::Max(UeremcpTop, Chosen[i].Score);
		if (Doc.CatalogOp.IsValid()) CatalogTop = FMath::Max(CatalogTop, Chosen[i].Score);
		for (const FString& M : Chosen[i].Matched) TopMatched.Add(M);
	}
	static const TSet<FString> Anchors = {
		TEXT("niagara"), TEXT("material"), TEXT("blueprint"), TEXT("montage"), TEXT("anim"),
		TEXT("spell"), TEXT("helix"), TEXT("capture"), TEXT("template"), TEXT("texture"),
		TEXT("projectile"), TEXT("emitter"), TEXT("vfx"), TEXT("graph"), TEXT("ability"),
		TEXT("gas"), TEXT("screenshot"), TEXT("viewport"), TEXT("job"), TEXT("plan"),
		TEXT("ribbon"), TEXT("particle"), TEXT("shader"),
		TEXT("landscape"), TEXT("terrain"), TEXT("river"), TEXT("forest"), TEXT("foliage"),
		TEXT("environment"), TEXT("weather"), TEXT("rain"), TEXT("mountain"), TEXT("biome")
	};
	bool bHasAnchor = false;
	for (const FString& M : TopMatched)
	{
		if (Anchors.Contains(M)) { bHasAnchor = true; break; }
	}
	const double TopScore = BestHitScore;
	const bool bStrong = FMath::Max(UeremcpTop, CatalogTop) >= 25.0
		&& (bHasAnchor || TopMatched.Num() >= 2);

	FString Confidence;
	FString ConfidenceReason;
	if (TopScore >= 25.0 && bStrong)
	{
		Confidence = TEXT("high");
		ConfidenceReason = TEXT("strong UEREMCP/catalog lexical match against live-registry candidates");
	}
	else if (FMath::Max(UeremcpTop, CatalogTop) > 0.0 || TopScore > 0.0)
	{
		Confidence = TEXT("low");
		ConfidenceReason = TEXT("weak or non-UEREMCP lexical signal; prefer clarification");
	}
	else
	{
		Confidence = TEXT("none");
		ConfidenceReason = TEXT("no registry candidate matched tokens");
	}
	Payload->SetStringField(TEXT("confidence"), Confidence);
	Payload->SetStringField(TEXT("confidence_reason"), ConfidenceReason);
	Payload->SetNumberField(TEXT("plan_score_floor"),
		FMath::RoundToFloat(static_cast<float>(PlanScoreFloor) * 10.0f) / 10.0f);

	const bool bAbstain = !bStrong;
	Payload->SetBoolField(TEXT("abstained"), bAbstain);

	auto AllowedInPlan = [](const FToolDoc& Doc) -> bool
	{
		if (Doc.bIsUeremcp || Doc.CatalogOp.IsValid()) return true;
		return Doc.Toolset.StartsWith(TEXT("re_agent_tools.toolsets.capture_workflow_tools"))
			|| Doc.Toolset.StartsWith(TEXT("EditorToolset.LogsToolset"));
	};

	TArray<TSharedPtr<FJsonValue>> PlanArr;
	// Default cap 3: single-domain intents must not emit five speculative steps (1.3d).
	//
	// But when the surviving candidates are CONNECTED by catalog dependency
	// edges they are a build sequence, not competing guesses, and truncating one
	// costs a whole round trip. Widen to 6 only on that evidence.
	int32 DefaultCap = 3;
	{
		TSet<FString> PoolActions;
		for (const FHit& H : Chosen)
		{
			const FToolDoc& D = Docs[H.DocIndex];
			if (!D.CatalogOp.IsValid()) continue;
			FString A;
			if (D.CatalogOp->TryGetStringField(TEXT("action"), A) && !A.IsEmpty())
			{
				PoolActions.Add(A);
			}
		}
		int32 Linked = 0;
		for (const FString& A : PoolActions)
		{
			if (const TArray<FString>* Preds = Catalog.DependsOn.Find(A))
			{
				for (const FString& P : *Preds)
				{
					if (PoolActions.Contains(P)) { ++Linked; break; }
				}
			}
		}
		if (Linked >= 2) DefaultCap = 6;
	}
	const int32 Cap = FMath::Clamp(MaxSteps > 0 ? MaxSteps : DefaultCap, 1, 12);
	if (!bAbstain)
	{
		int32 Step = 1;
		for (const FHit& H : Chosen)
		{
			if (Step > Cap) break;
			if (H.Score + KINDA_SMALL_NUMBER < PlanScoreFloor) continue;
			const FToolDoc& Doc = Docs[H.DocIndex];
			if (!Names.Contains(Doc.Qualified) || !AllowedInPlan(Doc))
			{
				continue;
			}
			TSharedPtr<FJsonObject> StepObj = MakeShared<FJsonObject>();
			StepObj->SetNumberField(TEXT("step"), Step++);
			StepObj->SetStringField(TEXT("toolset"), Doc.Toolset);
			StepObj->SetStringField(TEXT("tool"), Doc.Tool);
			StepObj->SetStringField(TEXT("qualified"), Doc.Qualified);
			StepObj->SetNumberField(TEXT("score"), FMath::RoundToFloat(static_cast<float>(H.Score) * 10.0f) / 10.0f);
			StepObj->SetStringField(TEXT("confidence"), Confidence);
			StepObj->SetStringField(TEXT("confidence_reason"), ConfidenceReason);
			StepObj->SetStringField(TEXT("why_here"), H.Why);
			TArray<TSharedPtr<FJsonValue>> Matched;
			for (const FString& M : H.Matched)
			{
				Matched.Add(MakeShared<FJsonValueString>(M));
				if (Matched.Num() >= 6) break;
			}
			StepObj->SetArrayField(TEXT("matched_terms"), Matched);
			StepObj->SetStringField(TEXT("purpose"), Doc.Description.Left(160));
			if (const TSharedPtr<FJsonObject> Nested =
					UeremcpSchemaPublishing::BuildNestedRequestSchemaForTool(Doc.Tool))
			{
				StepObj->SetObjectField(TEXT("input_schema"), Nested);
			}
			else
			{
				TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
				TArray<TSharedPtr<FJsonValue>> Props;
				for (const FString& P : Doc.Properties) Props.Add(MakeShared<FJsonValueString>(P));
				TArray<TSharedPtr<FJsonValue>> Req;
				for (const FString& R : Doc.Required) Req.Add(MakeShared<FJsonValueString>(R));
				Schema->SetArrayField(TEXT("properties"), Props);
				Schema->SetArrayField(TEXT("required"), Req);
				StepObj->SetObjectField(TEXT("input_schema"), Schema);
			}

			TSharedPtr<FJsonObject> Safety = MakeShared<FJsonObject>();
			Safety->SetBoolField(TEXT("destructive"), false);
			Safety->SetBoolField(TEXT("idempotent"), true);
			Safety->SetBoolField(TEXT("requires_revision"), false);
			Safety->SetBoolField(TEXT("prefer_dry_run"), Doc.bIsUeremcp);
			TArray<TSharedPtr<FJsonValue>> Missing;
			TArray<TSharedPtr<FJsonValue>> Statuses;
			if (Doc.CatalogOp.IsValid())
			{
				bool bDest = false, bIdem = true, bRev = false;
				Doc.CatalogOp->TryGetBoolField(TEXT("destructive"), bDest);
				Doc.CatalogOp->TryGetBoolField(TEXT("idempotent"), bIdem);
				Doc.CatalogOp->TryGetBoolField(TEXT("requires_revision"), bRev);
				Safety->SetBoolField(TEXT("destructive"), bDest);
				Safety->SetBoolField(TEXT("idempotent"), bIdem);
				Safety->SetBoolField(TEXT("requires_revision"), bRev);
				Safety->SetBoolField(TEXT("prefer_dry_run"), bDest || Doc.bIsUeremcp);
				const TSharedPtr<FJsonObject>* Example = nullptr;
				if (Doc.CatalogOp->TryGetObjectField(TEXT("example_request"), Example) && Example)
				{
					StepObj->SetObjectField(TEXT("request_json"), *Example);
				}
				FString Recovery;
				if (Doc.CatalogOp->TryGetStringField(TEXT("recovery"), Recovery))
				{
					StepObj->SetStringField(TEXT("recovery"), Recovery);
				}
				const TArray<TSharedPtr<FJsonValue>>* Exp = nullptr;
				if (Doc.CatalogOp->TryGetArrayField(TEXT("expected_statuses"), Exp) && Exp)
				{
					Statuses = *Exp;
				}
				if (bRev)
				{
					Missing.Add(MakeShared<FJsonValueString>(TEXT("expected_revision from prior read")));
				}
			}
			else if (Doc.bIsUeremcp)
			{
				TSharedPtr<FJsonObject> Example = MakeShared<FJsonObject>();
				Example->SetStringField(TEXT("protocol_version"), TEXT("1.0"));
				Example->SetStringField(TEXT("action"), TEXT("<see purpose>"));
				TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
				Target->SetStringField(TEXT("asset_path"), TEXT("/Game/__UeremcpTests/Foo"));
				Example->SetObjectField(TEXT("target"), Target);
				TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
				Options->SetBoolField(TEXT("dry_run"), true);
				Example->SetObjectField(TEXT("options"), Options);
				Example->SetObjectField(TEXT("specification"), MakeShared<FJsonObject>());
				StepObj->SetObjectField(TEXT("request_json"), Example);
				Missing.Add(MakeShared<FJsonValueString>(TEXT("action-specific specification fields")));
			}
			StepObj->SetObjectField(TEXT("safety"), Safety);
			StepObj->SetArrayField(TEXT("missing_fields"), Missing);
			StepObj->SetArrayField(TEXT("expected_statuses"), Statuses);
			if (!Doc.SupersededBy.IsEmpty())
			{
				StepObj->SetStringField(TEXT("warning"),
					FString::Printf(TEXT("superseded; prefer %s"), *Doc.SupersededBy));
				StepObj->SetStringField(TEXT("next_tool"), Doc.SupersededBy);
			}
			PlanArr.Add(MakeShared<FJsonValueObject>(StepObj));
		}
	}
	else
	{
		Payload->SetStringField(TEXT("fallback"),
			TEXT("no confident UEREMCP match; use list_toolsets/describe_toolset or answer clarification_questions"));
		TArray<TSharedPtr<FJsonValue>> Questions;
		Questions.Add(MakeShared<FJsonValueString>(
			TEXT("What asset type (Niagara, Material, Blueprint, Ability, Template)?")));
		Questions.Add(MakeShared<FJsonValueString>(
			TEXT("Is this create, inspect, modify, or visual-verify?")));
		Questions.Add(MakeShared<FJsonValueString>(
			TEXT("Do you already have an asset path under /Game/?")));
		Payload->SetArrayField(TEXT("clarification_questions"), Questions);
	}
	Payload->SetArrayField(TEXT("plan"), PlanArr);

	TArray<TSharedPtr<FJsonValue>> Alts;
	TSet<FString> Planned;
	for (const TSharedPtr<FJsonValue>& V : PlanArr)
	{
		FString Q;
		V->AsObject()->TryGetStringField(TEXT("qualified"), Q);
		Planned.Add(Q);
	}
	for (const FHit& H : Hits)
	{
		const FToolDoc& Doc = Docs[H.DocIndex];
		if (Planned.Contains(Doc.Qualified) || !Names.Contains(Doc.Qualified)) continue;
		TSharedPtr<FJsonObject> Alt = MakeShared<FJsonObject>();
		Alt->SetStringField(TEXT("tool"), Doc.Qualified);
		Alt->SetNumberField(TEXT("score"), FMath::RoundToFloat(static_cast<float>(H.Score) * 10.0f) / 10.0f);
		if (!Doc.SupersededBy.IsEmpty())
		{
			Alt->SetStringField(TEXT("superseded_by"), Doc.SupersededBy);
		}
		Alts.Add(MakeShared<FJsonValueObject>(Alt));
		if (Alts.Num() >= 5) break;
	}
	Payload->SetArrayField(TEXT("alternatives"), Alts);

	if (Context.IsValid())
	{
		Payload->SetObjectField(TEXT("context_echo"), Context);
	}

	Result.Status = TEXT("no_change_required");
	Result.Summary = bAbstain
		? TEXT("Abstained: low/none confidence — see clarification_questions")
		: FString::Printf(TEXT("Routed %d step(s) from live registry (confidence=%s)"), PlanArr.Num(), *Confidence);
	Result.CapabilityNotes.Add(
		TEXT("Routing accuracy is not end-to-end completion. Re-read tool results before claiming success."));
	Result.Payload = Payload;
	return Result;
}
