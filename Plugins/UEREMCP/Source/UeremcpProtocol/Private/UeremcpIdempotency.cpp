#include "UeremcpIdempotency.h"

FUeremcpIdempotencyStore& FUeremcpIdempotencyStore::Get()
{
	static FUeremcpIdempotencyStore Instance;
	return Instance;
}

bool FUeremcpIdempotencyStore::TryGet(const FString& Key, FString& OutResponseJson) const
{
	if (Key.IsEmpty())
	{
		return false;
	}
	if (const FString* Found = Entries.Find(Key))
	{
		OutResponseJson = *Found;
		return true;
	}
	return false;
}

void FUeremcpIdempotencyStore::Put(const FString& Key, const FString& ResponseJson)
{
	if (Key.IsEmpty())
	{
		return;
	}
	Entries.Add(Key, ResponseJson);
}

void FUeremcpIdempotencyStore::Clear()
{
	Entries.Empty();
}
