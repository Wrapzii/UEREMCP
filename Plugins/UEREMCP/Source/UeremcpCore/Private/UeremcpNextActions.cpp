// UEREMCP — serve the next step WITH the result.
//
// Measured problem: an agent finishing one call had no idea what to call next.
// Its options were to re-ask ResolveIntent -- a whole round trip to learn
// something the server already knew -- or to guess. Cost is superlinear in call
// count (docs/WHY.md), so a round trip spent on "what now?" is among the most
// expensive things the protocol can do.
//
// The catalog already declares the forward chain per operation. This serves it
// on the response, with THIS response's primary_asset already substituted into
// the next request. So after submit_mesh_ops the agent receives a
// scatter_foliage request with biome.mesh_path already filled in, and can send
// it without thinking, looking anything up, or asking.
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

	/** action -> its next_actions array. Parsed once; the catalog is static at runtime. */
	const TMap<FString, TArray<TSharedPtr<FJsonObject>>>& ChainByAction()
	{
		static TMap<FString, TArray<TSharedPtr<FJsonObject>>> Chain;
		static bool bLoaded = false;
		if (bLoaded)
		{
			return Chain;
		}
		bLoaded = true;

		FString Raw;
		const FString Path = CatalogPath();
		if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Raw, *Path))
		{
			return Chain;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return Chain;
		}
		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (!Root->TryGetArrayField(TEXT("operations"), Ops) || !Ops)
		{
			return Chain;
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
		return Chain;
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

	const TMap<FString, TArray<TSharedPtr<FJsonObject>>>& Chain = ChainByAction();
	const TArray<TSharedPtr<FJsonObject>>* Entries = Chain.Find(CompletedAction);
	if (!Entries)
	{
		return Out;
	}

	int32 Index = 0;
	for (const TSharedPtr<FJsonObject>& Entry : *Entries)
	{
		FString NextAction;
		if (!Entry->TryGetStringField(TEXT("action"), NextAction) || NextAction.IsEmpty())
		{
			continue;
		}
		FString Why;
		Entry->TryGetStringField(TEXT("why"), Why);

		const TSharedPtr<FJsonObject>* Hint = nullptr;
		Entry->TryGetObjectField(TEXT("specification_hint"), Hint);

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
		Suggestion->SetStringField(TEXT("why"), Why);
		Suggestion->SetObjectField(TEXT("request_json"), Request);
		Suggestion->SetStringField(TEXT("confidence"), Index == 0 ? TEXT("high") : TEXT("medium"));
		Out.Add(Suggestion);
		++Index;
	}
	return Out;
}
