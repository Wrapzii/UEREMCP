#include "UeremcpContentHash.h"

#include "UeremcpSha256.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
	bool IsGuidLikeKey(const FString& Key)
	{
		const FString Lower = Key.ToLower();
		return Lower.Equals(TEXT("guid"))
			|| Lower.EndsWith(TEXT("_guid"))
			|| Lower.Equals(TEXT("nodeguid"))
			|| Lower.Equals(TEXT("node_guid"))
			|| Lower.Equals(TEXT("pin_guid"));
	}

	bool IsIgnoredGraphField(const FString& Key)
	{
		return Key.Equals(TEXT("content_hash"))
			|| Key.Equals(TEXT("revision"))
			|| Key.Equals(TEXT("retrieved_at"))
			|| Key.Equals(TEXT("position"))
			|| Key.Equals(TEXT("bounds"))
			|| Key.Equals(TEXT("pin_id"))
			|| Key.Equals(TEXT("node_id"))
			|| Key.Equals(TEXT("comment_id"))
			|| IsGuidLikeKey(Key);
	}

	FString StableNodeKey(const TSharedPtr<FJsonObject>& Node)
	{
		FString SemanticId;
		if (Node->TryGetStringField(TEXT("semantic_id"), SemanticId) && !SemanticId.IsEmpty())
		{
			return FString::Printf(TEXT("sem:%s"), *SemanticId);
		}

		FString NodeClass;
		Node->TryGetStringField(TEXT("node_class"), NodeClass);
		FString SemanticType;
		Node->TryGetStringField(TEXT("semantic_type"), SemanticType);
		FString Title;
		Node->TryGetStringField(TEXT("title"), Title);
		return FString::Printf(TEXT("cls:%s|type:%s|title:%s"), *NodeClass, *SemanticType, *Title);
	}

	FString PinStableName(const TSharedPtr<FJsonObject>& Pin)
	{
		FString Name;
		Pin->TryGetStringField(TEXT("name"), Name);
		FString Direction;
		Pin->TryGetStringField(TEXT("direction"), Direction);
		return FString::Printf(TEXT("%s:%s"), *Direction, *Name);
	}

	TSharedPtr<FJsonValue> CanonicaliseValue(const TSharedPtr<FJsonValue>& Value);

	TSharedPtr<FJsonValue> CanonicalisePin(const TSharedPtr<FJsonObject>& Pin)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		TArray<FString> Keys;
		Keys.Reserve(Pin->Values.Num());
		for (const auto& Pair : Pin->Values)
		{
			Keys.Add(FString(Pair.Key));
		}
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			if (IsIgnoredGraphField(Key))
			{
				continue;
			}
			if (Key.Equals(TEXT("links")))
			{
				// Pin-level links duplicate top-level links[] and embed retrieval-local
				// node_id/pin_id. Omit them; connections are hashed via the edge list.
				continue;
			}
			Out->SetField(Key, CanonicaliseValue(Pin->TryGetField(Key)));
		}
		return MakeShared<FJsonValueObject>(Out);
	}

	TSharedPtr<FJsonValue> CanonicaliseNode(const TSharedPtr<FJsonObject>& Node)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("_stable_key"), StableNodeKey(Node));

		TArray<FString> Keys;
		Keys.Reserve(Node->Values.Num());
		for (const auto& Pair : Node->Values)
		{
			Keys.Add(FString(Pair.Key));
		}
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			if (IsIgnoredGraphField(Key) || Key.Equals(TEXT("semantic_id")))
			{
				// semantic_id is folded into _stable_key
				continue;
			}
			if (Key.Equals(TEXT("input_pins")) || Key.Equals(TEXT("output_pins")))
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (!Node->TryGetArrayField(Key, Arr) || !Arr)
				{
					continue;
				}
				TArray<TSharedPtr<FJsonValue>> CanonPins;
				for (const TSharedPtr<FJsonValue>& PinVal : *Arr)
				{
					if (PinVal.IsValid() && PinVal->Type == EJson::Object)
					{
						CanonPins.Add(CanonicalisePin(PinVal->AsObject()));
					}
				}
				CanonPins.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
				{
					const FString KA = A->AsObject()->GetStringField(TEXT("direction"))
						+ TEXT("|") + A->AsObject()->GetStringField(TEXT("name"));
					const FString KB = B->AsObject()->GetStringField(TEXT("direction"))
						+ TEXT("|") + B->AsObject()->GetStringField(TEXT("name"));
					return KA < KB;
				});
				Out->SetArrayField(Key, CanonPins);
				continue;
			}
			Out->SetField(Key, CanonicaliseValue(Node->TryGetField(Key)));
		}
		return MakeShared<FJsonValueObject>(Out);
	}

	TSharedPtr<FJsonValue> CanonicaliseGraphObject(const TSharedPtr<FJsonObject>& Obj)
	{
		// Build node_id -> stable key map for rewriting top-level links.
		TMap<FString, FString> NodeIdToStable;
		TMap<FString, TMap<FString, FString>> NodePinIdToName; // node_id -> pin_id -> name:dir

		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (Obj->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
		{
			for (const TSharedPtr<FJsonValue>& NodeVal : *Nodes)
			{
				if (!NodeVal.IsValid() || NodeVal->Type != EJson::Object)
				{
					continue;
				}
				const TSharedPtr<FJsonObject> Node = NodeVal->AsObject();
				FString NodeId;
				Node->TryGetStringField(TEXT("node_id"), NodeId);
				const FString Stable = StableNodeKey(Node);
				if (!NodeId.IsEmpty())
				{
					NodeIdToStable.Add(NodeId, Stable);
				}

				auto IndexPins = [&](const FString& Field)
				{
					const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
					if (!Node->TryGetArrayField(Field, Pins) || !Pins)
					{
						return;
					}
					for (const TSharedPtr<FJsonValue>& PinVal : *Pins)
					{
						if (!PinVal.IsValid() || PinVal->Type != EJson::Object)
						{
							continue;
						}
						const TSharedPtr<FJsonObject> Pin = PinVal->AsObject();
						FString PinId;
						Pin->TryGetStringField(TEXT("pin_id"), PinId);
						if (!NodeId.IsEmpty() && !PinId.IsEmpty())
						{
							NodePinIdToName.FindOrAdd(NodeId).Add(PinId, PinStableName(Pin));
						}
					}
				};
				IndexPins(TEXT("input_pins"));
				IndexPins(TEXT("output_pins"));
			}
		}

		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		TArray<FString> Keys;
		Keys.Reserve(Obj->Values.Num());
		for (const auto& Pair : Obj->Values)
		{
			Keys.Add(FString(Pair.Key));
		}
		Keys.Sort();

		for (const FString& Key : Keys)
		{
			if (IsIgnoredGraphField(Key))
			{
				continue;
			}

			if (Key.Equals(TEXT("nodes")) && Nodes)
			{
				TArray<TSharedPtr<FJsonValue>> CanonNodes;
				for (const TSharedPtr<FJsonValue>& NodeVal : *Nodes)
				{
					if (NodeVal.IsValid() && NodeVal->Type == EJson::Object)
					{
						CanonNodes.Add(CanonicaliseNode(NodeVal->AsObject()));
					}
				}
				CanonNodes.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
				{
					return A->AsObject()->GetStringField(TEXT("_stable_key"))
						< B->AsObject()->GetStringField(TEXT("_stable_key"));
				});
				Out->SetArrayField(TEXT("nodes"), CanonNodes);
				continue;
			}

			if (Key.Equals(TEXT("links")))
			{
				const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
				if (!Obj->TryGetArrayField(TEXT("links"), Links) || !Links)
				{
					continue;
				}
				TArray<TSharedPtr<FJsonValue>> CanonLinks;
				for (const TSharedPtr<FJsonValue>& LinkVal : *Links)
				{
					if (!LinkVal.IsValid() || LinkVal->Type != EJson::Object)
					{
						continue;
					}
					const TSharedPtr<FJsonObject> Link = LinkVal->AsObject();
					FString FromNode, FromPin, ToNode, ToPin, Kind;
					Link->TryGetStringField(TEXT("from_node"), FromNode);
					Link->TryGetStringField(TEXT("from_pin"), FromPin);
					Link->TryGetStringField(TEXT("to_node"), ToNode);
					Link->TryGetStringField(TEXT("to_pin"), ToPin);
					Link->TryGetStringField(TEXT("kind"), Kind);

					const FString* FromStable = NodeIdToStable.Find(FromNode);
					const FString* ToStable = NodeIdToStable.Find(ToNode);
					FString FromPinStable = FromPin;
					FString ToPinStable = ToPin;
					if (const TMap<FString, FString>* PinMap = NodePinIdToName.Find(FromNode))
					{
						if (const FString* N = PinMap->Find(FromPin))
						{
							FromPinStable = *N;
						}
					}
					if (const TMap<FString, FString>* PinMap = NodePinIdToName.Find(ToNode))
					{
						if (const FString* N = PinMap->Find(ToPin))
						{
							ToPinStable = *N;
						}
					}

					TSharedPtr<FJsonObject> CL = MakeShared<FJsonObject>();
					CL->SetStringField(TEXT("from_node"), FromStable ? *FromStable : FromNode);
					CL->SetStringField(TEXT("from_pin"), FromPinStable);
					CL->SetStringField(TEXT("to_node"), ToStable ? *ToStable : ToNode);
					CL->SetStringField(TEXT("to_pin"), ToPinStable);
					if (!Kind.IsEmpty())
					{
						CL->SetStringField(TEXT("kind"), Kind);
					}
					CanonLinks.Add(MakeShared<FJsonValueObject>(CL));
				}
				CanonLinks.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
				{
					auto KeyOf = [](const TSharedPtr<FJsonValue>& V) -> FString
					{
						const TSharedPtr<FJsonObject> O = V->AsObject();
						return O->GetStringField(TEXT("from_node")) + TEXT(">")
							+ O->GetStringField(TEXT("from_pin")) + TEXT(">")
							+ O->GetStringField(TEXT("to_node")) + TEXT(">")
							+ O->GetStringField(TEXT("to_pin")) + TEXT(">")
							+ O->GetStringField(TEXT("kind"));
					};
					return KeyOf(A) < KeyOf(B);
				});
				Out->SetArrayField(TEXT("links"), CanonLinks);
				continue;
			}

			Out->SetField(Key, CanonicaliseValue(Obj->TryGetField(Key)));
		}
		return MakeShared<FJsonValueObject>(Out);
	}

	TSharedPtr<FJsonValue> CanonicaliseValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return MakeShared<FJsonValueNull>();
		}
		switch (Value->Type)
		{
		case EJson::Boolean:
			return MakeShared<FJsonValueBoolean>(Value->AsBool());
		case EJson::Number:
			return MakeShared<FJsonValueNumber>(Value->AsNumber());
		case EJson::String:
			return MakeShared<FJsonValueString>(Value->AsString());
		case EJson::Array:
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				Out.Add(CanonicaliseValue(Item));
			}
			return MakeShared<FJsonValueArray>(Out);
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			// Graph-shaped objects get special treatment.
			if (Obj->HasField(TEXT("nodes")) || Obj->HasField(TEXT("graph_type")))
			{
				return CanonicaliseGraphObject(Obj);
			}
			TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
			TArray<FString> Keys;
			Keys.Reserve(Obj->Values.Num());
			for (const auto& Pair : Obj->Values)
			{
				Keys.Add(FString(Pair.Key));
			}
			Keys.Sort();
			for (const FString& Key : Keys)
			{
				if (IsIgnoredGraphField(Key))
				{
					continue;
				}
				Out->SetField(Key, CanonicaliseValue(Obj->TryGetField(Key)));
			}
			return MakeShared<FJsonValueObject>(Out);
		}
		default:
			return MakeShared<FJsonValueNull>();
		}
	}


	TSharedPtr<FJsonValue> SortJsonKeys(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return Value;
		}
		switch (Value->Type)
		{
		case EJson::Array:
		{
			TArray<TSharedPtr<FJsonValue>> Out;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				Out.Add(SortJsonKeys(Item));
			}
			return MakeShared<FJsonValueArray>(Out);
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			TArray<FString> Keys;
			Keys.Reserve(Obj->Values.Num());
			for (const auto& Pair : Obj->Values)
			{
				Keys.Add(FString(Pair.Key));
			}
			Keys.Sort();
			TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
			for (const FString& Key : Keys)
			{
				Out->SetField(Key, SortJsonKeys(Obj->TryGetField(Key)));
			}
			return MakeShared<FJsonValueObject>(Out);
		}
		default:
			return Value;
		}
	}

	bool WriteCanonical(const TSharedPtr<FJsonValue>& Value, FString& Out)
	{
		// Condensed JSON with sorted object keys already applied by canonicalise.
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		const bool bOk = FJsonSerializer::Serialize(Value, TEXT(""), Writer);
		Writer->Close();
		return bOk;
	}
}

TSharedPtr<FJsonValue> FUeremcpContentHash::CanonicaliseForHash(const TSharedPtr<FJsonValue>& Value)
{
	return SortJsonKeys(CanonicaliseValue(Value));
}

FString FUeremcpContentHash::Sha256Prefixed(const TArray<uint8>& Bytes)
{
	uint8 Digest[UeremcpSha256::DigestBytes];
	UeremcpSha256::Hash(Bytes.GetData(), Bytes.Num(), Digest);
	return FString::Printf(TEXT("sha256:%s"), *UeremcpSha256::ToHex(Digest));
}

FString FUeremcpContentHash::Sha256Prefixed(const FString& Utf8Text)
{
	FTCHARToUTF8 Converter(*Utf8Text);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	return Sha256Prefixed(Bytes);
}

FString FUeremcpContentHash::HashJsonValue(const TSharedPtr<FJsonValue>& Value, FString* OutError)
{
	if (!Value.IsValid())
	{
		if (OutError)
		{
			*OutError = TEXT("null JSON value");
		}
		return FString();
	}
	const TSharedPtr<FJsonValue> Canon = CanonicaliseForHash(Value);
	FString CanonicalJson;
	if (!WriteCanonical(Canon, CanonicalJson))
	{
		if (OutError)
		{
			*OutError = TEXT("failed to serialise canonical JSON");
		}
		return FString();
	}
	return Sha256Prefixed(CanonicalJson);
}

FString FUeremcpContentHash::HashJsonObject(const TSharedPtr<FJsonObject>& Object, FString* OutError)
{
	if (!Object.IsValid())
	{
		if (OutError)
		{
			*OutError = TEXT("null JSON object");
		}
		return FString();
	}
	return HashJsonValue(MakeShared<FJsonValueObject>(Object), OutError);
}

FString FUeremcpContentHash::HashJsonString(const FString& Json, FString* OutError)
{
	TSharedPtr<FJsonValue> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		if (OutError)
		{
			*OutError = TEXT("invalid JSON");
		}
		return FString();
	}
	return HashJsonValue(Root, OutError);
}
