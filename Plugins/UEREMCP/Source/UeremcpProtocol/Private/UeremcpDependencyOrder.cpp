#include "UeremcpDependencyOrder.h"

bool FUeremcpDependencyOrder::TopologicalSort(
	const TArray<FUeremcpDependencyNode>& Nodes,
	TArray<FString>& OutOrderedIds,
	FString& OutError)
{
	OutOrderedIds.Reset();
	OutError.Reset();

	TMap<FString, int32> IndexById;
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const FString& Id = Nodes[I].Id;
		if (Id.IsEmpty())
		{
			OutError = TEXT("dependency node has empty id");
			return false;
		}
		if (IndexById.Contains(Id))
		{
			OutError = FString::Printf(TEXT("duplicate operation id '%s'"), *Id);
			return false;
		}
		IndexById.Add(Id, I);
	}

	TMap<FString, int32> InDegree;
	TMap<FString, TArray<FString>> Dependents; // dep -> ops that depend on it
	TArray<FString> InsertionOrder;

	for (const FUeremcpDependencyNode& Node : Nodes)
	{
		InDegree.FindOrAdd(Node.Id) = 0;
		InsertionOrder.Add(Node.Id);
	}

	for (const FUeremcpDependencyNode& Node : Nodes)
	{
		TSet<FString> SeenDeps;
		for (const FString& Dep : Node.DependsOn)
		{
			if (Dep.IsEmpty())
			{
				OutError = FString::Printf(TEXT("operation '%s' has empty depends_on entry"), *Node.Id);
				return false;
			}
			if (!IndexById.Contains(Dep))
			{
				OutError = FString::Printf(
					TEXT("operation '%s' depends_on unknown id '%s'"), *Node.Id, *Dep);
				return false;
			}
			if (SeenDeps.Contains(Dep))
			{
				continue; // ignore duplicate edges
			}
			SeenDeps.Add(Dep);
			InDegree.FindOrAdd(Node.Id)++;
			Dependents.FindOrAdd(Dep).Add(Node.Id);
		}
	}

	TArray<FString> Ready;
	for (const FString& Id : InsertionOrder)
	{
		if (InDegree.FindRef(Id) == 0)
		{
			Ready.Add(Id);
		}
	}

	while (Ready.Num() > 0)
	{
		const FString Id = Ready[0];
		Ready.RemoveAt(0);
		OutOrderedIds.Add(Id);

		if (const TArray<FString>* Kids = Dependents.Find(Id))
		{
			for (const FString& Kid : *Kids)
			{
				int32& Deg = InDegree.FindChecked(Kid);
				--Deg;
				if (Deg == 0)
				{
					Ready.Add(Kid);
				}
			}
		}
	}

	if (OutOrderedIds.Num() != Nodes.Num())
	{
		TArray<FString> CycleMembers;
		for (const FString& Id : InsertionOrder)
		{
			if (InDegree.FindRef(Id) > 0)
			{
				CycleMembers.Add(Id);
			}
		}
		OutError = FString::Printf(
			TEXT("dependency cycle involving: %s"), *FString::Join(CycleMembers, TEXT(", ")));
		OutOrderedIds.Reset();
		return false;
	}

	return true;
}
