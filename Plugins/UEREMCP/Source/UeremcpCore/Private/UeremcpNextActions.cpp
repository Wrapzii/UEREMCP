// UEREMCP — serve the next step WITH the result.
//
// Measured problem: an agent finishing one call had no idea what to call next.
// Its options were to re-ask ResolveIntent -- a whole round trip to learn
// something the server already knew -- or to guess. Cost is superlinear in call
// count (docs/WHY.md), so a round trip spent on "what now?" is among the most
// expensive things the protocol can do.
//
// What gets offered is the GRAPH NEIGHBOURHOOD, not a single forward edge:
//
//   implied     the declared next step -- what usually follows
//   consumes    one layer DOWN: what this is built from
//   consumed_by one layer UP: what is built from this
//
// A single forward edge only helps an agent already going the right way. The
// neighbourhood also covers "I have the material, I still need its texture"
// (down) and "I have the mesh, what wants a mesh?" (up). Pairs become
// bidirectional for free: material offers texture, texture offers material.
//
// Up and down are DERIVED by inverting depends_on_actions, so they cost no
// hand-maintained table and cannot disagree with the dependency graph the
// router and execute_plan already use.
//
// Each carries THIS response's primary_asset already substituted, so after
// submit_mesh_ops the agent receives a scatter_foliage request with
// biome.mesh_path filled in and can send it without looking anything up.
//
// Advisory, never binding. The agent may ignore every suggestion.

#include "UeremcpNextActions.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/** Same file the router reads. One catalog, or the two drift -- which they did. */
	FString CatalogPath()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UEREMCP")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(),
				TEXT("Content/IntentRouter/operation_catalog.json"));
		}
		return FString();
	}

	struct FGraph
	{
		/** action -> declared next_actions entries */
		TMap<FString, TArray<TSharedPtr<FJsonObject>>> Chain;
		/** action -> what it is built FROM (one layer down) */
		TMap<FString, TArray<FString>> Consumes;
		/** action -> what is built FROM IT (one layer up) */
		TMap<FString, TArray<FString>> ConsumedBy;
		/** action -> its `why` from the dependency graph, for explaining an edge */
		TMap<FString, FString> Why;
	};

	/** Parsed once; the catalog is static at runtime. */
	const FGraph& Graph()
	{
		static FGraph G;
		static bool bLoaded = false;
		if (bLoaded)
		{
			return G;
		}
		bLoaded = true;
		TMap<FString, TArray<TSharedPtr<FJsonObject>>>& Chain = G.Chain;

		FString Raw;
		const FString Path = CatalogPath();
		if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Raw, *Path))
		{
			return G;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return G;
		}

		// Derive both directions from the one dependency graph. A separate
		// hand-written "what comes after" table would drift from it, and the
		// drift would be invisible until an agent followed a stale edge.
		const TArray<TSharedPtr<FJsonValue>>* Deps = nullptr;
		if (Root->TryGetArrayField(TEXT("dependencies"), Deps) && Deps)
		{
			for (const TSharedPtr<FJsonValue>& V : *Deps)
			{
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) continue;
				FString Action;
				if (!Obj->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty()) continue;
				FString Why;
				if (Obj->TryGetStringField(TEXT("why"), Why)) G.Why.Add(Action, Why);
				const TArray<TSharedPtr<FJsonValue>>* Parents = nullptr;
				if (!Obj->TryGetArrayField(TEXT("depends_on_actions"), Parents) || !Parents) continue;
				for (const TSharedPtr<FJsonValue>& P : *Parents)
				{
					const FString Parent = P->AsString();
					if (Parent.IsEmpty()) continue;
					G.Consumes.FindOrAdd(Action).AddUnique(Parent);
					G.ConsumedBy.FindOrAdd(Parent).AddUnique(Action);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (!Root->TryGetArrayField(TEXT("operations"), Ops) || !Ops)
		{
			return G;
		}
		for (const TSharedPtr<FJsonValue>& V : *Ops)
		{
			const TSharedPtr<FJsonObject> Op = V->AsObject();
			if (!Op.IsValid()) continue;
			FString Action;
			if (!Op->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty()) continue;
			const TArray<TSharedPtr<FJsonValue>>* Next = nullptr;
			if (!Op->TryGetArrayField(TEXT("next_actions"), Next) || !Next) continue;

			TArray<TSharedPtr<FJsonObject>> Entries;
			for (const TSharedPtr<FJsonValue>& N : *Next)
			{
				if (const TSharedPtr<FJsonObject> Obj = N->AsObject())
				{
					Entries.Add(Obj);
				}
			}
			if (Entries.Num() > 0)
			{
				Chain.Add(Action, MoveTemp(Entries));
			}
		}
		return G;
	}

	/**
	 * Replace the "<primary_asset>" placeholder anywhere in a spec.
	 *
	 * This substitution is the whole value: a suggestion the agent must still
	 * fill in is a suggestion it has to think about, and thinking is where the
	 * wrong path gets chosen.
	 */
	TSharedPtr<FJsonObject> Substitute(
		const TSharedPtr<FJsonObject>& Spec,
		const FString& PrimaryAsset)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		if (!Spec.IsValid())
		{
			return Out;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Spec->Values)
		{
			const TSharedPtr<FJsonValue>& Value = Pair.Value;
			if (!Value.IsValid())
			{
				continue;
			}
			if (Value->Type == EJson::String)
			{
				FString Str = Value->AsString();
				if (Str.Equals(TEXT("<primary_asset>")))
				{
					if (PrimaryAsset.IsEmpty())
					{
						continue;  // nothing to fill; omit rather than emit a placeholder
					}
					Str = PrimaryAsset;
				}
				Out->SetStringField(Pair.Key, Str);
			}
			else if (Value->Type == EJson::Object)
			{
				Out->SetObjectField(Pair.Key, Substitute(Value->AsObject(), PrimaryAsset));
			}
			else
			{
				Out->SetField(Pair.Key, Value);
			}
		}
		return Out;
	}
}

TArray<TSharedPtr<FJsonObject>> FUeremcpNextActions::Suggest(
	const FString& CompletedAction,
	const FString& PrimaryAsset,
	const FString& Status)
{
	TArray<TSharedPtr<FJsonObject>> Out;

	// Never suggest a next step off a failure. "You succeeded, now do X" is
	// actively misleading when nothing was produced, and a rejection already
	// carries the reason the agent needs to act on instead.
	static const TSet<FString> TerminalFailures = {
		TEXT("rejected"), TEXT("error"), TEXT("failed_validation"), TEXT("rolled_back")
	};
	if (TerminalFailures.Contains(Status))
	{
		return Out;
	}

	const FGraph& G = Graph();

	// One entry per action, best-first. An action reachable by two relations is
	// offered once, under the strongest -- repeating it as three "options" makes
	// a two-choice decision look like a six-choice one.
	TSet<FString> Emitted;
	Emitted.Add(CompletedAction);

	auto Add = [&](const FString& NextAction, const FString& Relation,
	               const FString& Why, const TSharedPtr<FJsonObject>* Hint,
	               const TCHAR* Confidence)
	{
		if (NextAction.IsEmpty() || Emitted.Contains(NextAction))
		{
			return;
		}
		Emitted.Add(NextAction);

		TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TEXT("protocol_version"), TEXT("1.0"));
		Request->SetStringField(TEXT("action"), NextAction);
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), true);
		Request->SetObjectField(TEXT("options"), Options);
		Request->SetObjectField(TEXT("specification"),
			Substitute(Hint ? *Hint : nullptr, PrimaryAsset));

		TSharedPtr<FJsonObject> Suggestion = MakeShared<FJsonObject>();
		Suggestion->SetStringField(TEXT("action"), NextAction);
		Suggestion->SetStringField(TEXT("relation"), Relation);
		Suggestion->SetStringField(TEXT("why"), Why);
		Suggestion->SetObjectField(TEXT("request_json"), Request);
		Suggestion->SetStringField(TEXT("confidence"), Confidence);
		Out.Add(Suggestion);
	};

	// 1. IMPLIED — the declared next step. Highest confidence: it carries a
	//    specification_hint, so its request arrives ready to send.
	if (const TArray<TSharedPtr<FJsonObject>>* Entries = G.Chain.Find(CompletedAction))
	{
		int32 Index = 0;
		for (const TSharedPtr<FJsonObject>& Entry : *Entries)
		{
			FString NextAction;
			if (!Entry->TryGetStringField(TEXT("action"), NextAction)) continue;
			FString Why;
			Entry->TryGetStringField(TEXT("why"), Why);
			const TSharedPtr<FJsonObject>* Hint = nullptr;
			Entry->TryGetObjectField(TEXT("specification_hint"), Hint);
			Add(NextAction, TEXT("implied"), Why, Hint,
				Index == 0 ? TEXT("high") : TEXT("medium"));
			++Index;
		}
	}

	// 2. UP — what is built FROM this. "I have a mesh; what wants a mesh?"
	if (const TArray<FString>* Up = G.ConsumedBy.Find(CompletedAction))
	{
		for (const FString& Consumer : *Up)
		{
			const FString* EdgeWhy = G.Why.Find(Consumer);
			Add(Consumer, TEXT("consumed_by"),
				EdgeWhy ? *EdgeWhy
				        : FString::Printf(TEXT("%s consumes what you just made"), *Consumer),
				nullptr, TEXT("medium"));
		}
	}

	// 3. DOWN — what this is built FROM. Offered even though it is "behind" the
	//    agent, because the common failure is having the thing and still lacking
	//    its inputs: a master material with no texture, a scatter with no mesh.
	if (const TArray<FString>* Down = G.Consumes.Find(CompletedAction))
	{
		const FString* EdgeWhy = G.Why.Find(CompletedAction);
		for (const FString& Input : *Down)
		{
			Add(Input, TEXT("consumes"),
				EdgeWhy ? *EdgeWhy
				        : FString::Printf(TEXT("%s supplies an input this consumes"), *Input),
				nullptr, TEXT("low"));
		}
	}

	// A menu longer than a handful stops being guidance and becomes a listing.
	if (Out.Num() > 5)
	{
		Out.SetNum(5);
	}
	return Out;
}
