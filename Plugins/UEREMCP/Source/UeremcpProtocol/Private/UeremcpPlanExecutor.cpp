#include "UeremcpPlanExecutor.h"

#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UeremcpDependencyOrder.h"
#include "UeremcpEnvelope.h"
#include "UeremcpRefResolve.h"

namespace
{
	using FHandlerPtr = TSharedPtr<FUeremcpPlanOperationHandler, ESPMode::ThreadSafe>;
	using FTransactionPtr = TSharedPtr<FUeremcpPlanTransactionCallbacks, ESPMode::ThreadSafe>;

	FCriticalSection GPlanMutex;
	TMap<FString, FHandlerPtr> GHandlers;
	FTransactionPtr GTransaction;

	struct FOperation
	{
		FString Id;
		FString Action;
		TArray<FString> DependsOn;
		TSharedPtr<FJsonObject> Object;
		bool bOptional = false;
	};

	bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& Out)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}

	bool SerializeObject(const TSharedPtr<FJsonObject>& Object, FString& Out)
	{
		Out.Reset();
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		return Object.IsValid() && FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	}

	TSharedPtr<FJsonObject> BaseResponse(
		const FString& RequestId,
		const FString& Status,
		const FString& Summary)
	{
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("protocol_version"), FUeremcpEnvelope::ProtocolVersion());
		if (!RequestId.IsEmpty())
		{
			Response->SetStringField(TEXT("request_id"), RequestId);
		}
		Response->SetStringField(TEXT("status"), Status);
		Response->SetStringField(TEXT("summary"), Summary);
		TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
		Metrics->SetNumberField(TEXT("mcp_round_trips"), 1);
		Metrics->SetNumberField(TEXT("internal_operations"), 0);
		Response->SetObjectField(TEXT("metrics"), Metrics);
		return Response;
	}

	bool Return(
		const TSharedPtr<FJsonObject>& Response,
		FString& OutResponse,
		FString& OutError)
	{
		if (!SerializeObject(Response, OutResponse))
		{
			OutError = TEXT("failed to serialize execute_plan response");
			return false;
		}
		OutError.Reset();
		return true;
	}

	bool Reject(
		const FString& RequestId,
		const FString& Reason,
		FString& OutResponse,
		FString& OutError)
	{
		return Return(BaseResponse(RequestId, TEXT("rejected"), Reason), OutResponse, OutError);
	}

	bool IsSuccess(const FString& Status)
	{
		return Status == TEXT("created_and_validated")
			|| Status == TEXT("modified_and_validated")
			|| Status == TEXT("created_with_warnings")
			|| Status == TEXT("no_change_required");
	}

	bool IsUsablePartial(
		const FString& Status,
		const TSharedPtr<FJsonObject>& Response)
	{
		const TSharedPtr<FJsonObject>* Result = nullptr;
		FString PrimaryAsset;
		return Status == TEXT("partially_completed")
			&& Response.IsValid()
			&& Response->TryGetObjectField(TEXT("result"), Result)
			&& Result
			&& (*Result)->TryGetStringField(TEXT("primary_asset"), PrimaryAsset)
			&& !PrimaryAsset.IsEmpty();
	}

	bool ValidAction(const FString& Action)
	{
		if (Action.IsEmpty() || Action[0] < TEXT('a') || Action[0] > TEXT('z'))
		{
			return false;
		}
		for (int32 Index = 1; Index < Action.Len(); ++Index)
		{
			const TCHAR Char = Action[Index];
			if (!((Char >= TEXT('a') && Char <= TEXT('z'))
				|| (Char >= TEXT('0') && Char <= TEXT('9'))
				|| Char == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	bool ReadDependencies(
		const TSharedPtr<FJsonObject>& Object,
		TArray<FString>& Out,
		FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(TEXT("depends_on"), Values))
		{
			return true;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Dependency;
			if (!Value.IsValid() || !Value->TryGetString(Dependency) || Dependency.IsEmpty())
			{
				OutError = TEXT("depends_on entries must be non-empty strings");
				return false;
			}
			Out.Add(Dependency);
		}
		return true;
	}

	bool ParseOperations(
		const TSharedPtr<FJsonObject>& Specification,
		TArray<FOperation>& OutOperations,
		TArray<FString>& OutOrder,
		FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Specification.IsValid()
			|| !Specification->TryGetArrayField(TEXT("operations"), Values)
			|| !Values
			|| Values->IsEmpty())
		{
			OutError = TEXT("execute_plan specification.operations must be a non-empty array");
			return false;
		}

		TArray<FUeremcpDependencyNode> Nodes;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !Object->IsValid())
			{
				OutError = TEXT("execute_plan operations must be objects");
				return false;
			}
			FOperation Operation;
			Operation.Object = *Object;
			if (!Operation.Object->TryGetStringField(TEXT("id"), Operation.Id)
				|| Operation.Id.IsEmpty()
				|| !Operation.Object->TryGetStringField(TEXT("action"), Operation.Action)
				|| !ValidAction(Operation.Action)
				|| Operation.Action == TEXT("execute_plan"))
			{
				OutError = TEXT("every operation requires an id and valid non-recursive action");
				return false;
			}
			if (!ReadDependencies(Operation.Object, Operation.DependsOn, OutError))
			{
				return false;
			}
			Operation.Object->TryGetBoolField(TEXT("optional"), Operation.bOptional);
			const TSharedPtr<FJsonObject>* Condition = nullptr;
			if (Operation.Object->TryGetObjectField(TEXT("condition"), Condition)
				&& Condition
				&& ((*Condition)->HasField(TEXT("asset_exists"))
					|| (*Condition)->HasField(TEXT("asset_missing"))))
			{
				OutError = FString::Printf(
					TEXT("operation '%s' uses an asset condition without an evaluator"),
					*Operation.Id);
				return false;
			}

			FUeremcpDependencyNode Node;
			Node.Id = Operation.Id;
			Node.DependsOn = Operation.DependsOn;
			Nodes.Add(MoveTemp(Node));
			OutOperations.Add(MoveTemp(Operation));
		}
		return FUeremcpDependencyOrder::TopologicalSort(Nodes, OutOrder, OutError);
	}

	bool ConditionAllows(
		const FOperation& Operation,
		const TMap<FString, FString>& StatusById,
		bool& bOutRun,
		FString& OutReason,
		FString& OutError)
	{
		bOutRun = true;
		const TSharedPtr<FJsonObject>* Condition = nullptr;
		if (!Operation.Object->TryGetObjectField(TEXT("condition"), Condition))
		{
			return true;
		}
		const TSharedPtr<FJsonObject>* StatusCondition = nullptr;
		if (!(*Condition)->TryGetObjectField(TEXT("operation_status"), StatusCondition))
		{
			return true;
		}
		FString Id;
		const TArray<TSharedPtr<FJsonValue>>* Allowed = nullptr;
		if (!(*StatusCondition)->TryGetStringField(TEXT("id"), Id)
			|| !(*StatusCondition)->TryGetArrayField(TEXT("is"), Allowed)
			|| !Allowed)
		{
			OutError = FString::Printf(
				TEXT("operation '%s' has malformed operation_status condition"),
				*Operation.Id);
			return false;
		}
		const FString* Actual = StatusById.Find(Id);
		if (!Actual)
		{
			OutError = FString::Printf(
				TEXT("operation '%s' condition references unavailable operation '%s'"),
				*Operation.Id,
				*Id);
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Allowed)
		{
			FString Candidate;
			if (Value.IsValid() && Value->TryGetString(Candidate) && Candidate == *Actual)
			{
				return true;
			}
		}
		bOutRun = false;
		OutReason = FString::Printf(
			TEXT("condition not met: operation '%s' status was '%s'"), *Id, **Actual);
		return true;
	}

	bool ShouldCompile(
		const FString& Policy,
		const FOperation& Operation,
		const TArray<FOperation>& Operations,
		int32 Index,
		int32 Count)
	{
		if (Policy == TEXT("never"))
		{
			return false;
		}
		if (Policy == TEXT("per_operation"))
		{
			return true;
		}
		if (Policy == TEXT("at_end"))
		{
			return Index == Count - 1;
		}
		for (const FOperation& Other : Operations)
		{
			if (Other.DependsOn.Contains(Operation.Id))
			{
				return true;
			}
		}
		return Index == Count - 1;
	}

	bool BuildOperationRequest(
		const TSharedPtr<FJsonObject>& Root,
		const FOperation& Operation,
		const TMap<FString, TSharedPtr<FJsonObject>>& Completed,
		bool bCompile,
		bool bValidate,
		FString& OutJson,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TEXT("protocol_version"), FUeremcpEnvelope::ProtocolVersion());
		FString RequestId;
		if (Root->TryGetStringField(TEXT("request_id"), RequestId))
		{
			Request->SetStringField(TEXT("request_id"), RequestId + TEXT(":") + Operation.Id);
		}
		Request->SetStringField(TEXT("action"), Operation.Action);
		for (const FString& Field : {
			TEXT("project"), TEXT("target"), TEXT("mode"), TEXT("expected_revision"), TEXT("idempotency_key") })
		{
			if (Root->HasField(Field))
			{
				Request->SetField(Field, Root->TryGetField(Field));
			}
		}
		for (const FString& Field : { TEXT("target"), TEXT("mode"), TEXT("expected_revision") })
		{
			if (Operation.Object->HasField(Field))
			{
				Request->SetField(Field, Operation.Object->TryGetField(Field));
			}
		}

		TSharedPtr<FJsonObject> Specification = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* Existing = nullptr;
		if (Operation.Object->TryGetObjectField(TEXT("specification"), Existing)
			&& Existing && Existing->IsValid())
		{
			Specification = *Existing;
		}
		TSharedPtr<FJsonValue> SpecificationValue = MakeShared<FJsonValueObject>(Specification);
		if (!FUeremcpRefResolve::ResolveInPlace(SpecificationValue, Completed, OutError))
		{
			return false;
		}
		Request->SetObjectField(TEXT("specification"), SpecificationValue->AsObject());

		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* ParentOptions = nullptr;
		if (Root->TryGetObjectField(TEXT("options"), ParentOptions)
			&& ParentOptions && ParentOptions->IsValid())
		{
			for (const auto& Pair : (*ParentOptions)->Values)
			{
				Options->SetField(FString(Pair.Key), Pair.Value);
			}
		}
		Options->SetBoolField(TEXT("compile"), bCompile);
		Options->SetBoolField(TEXT("validate"), bValidate);
		Options->SetNumberField(TEXT("timeout_ms"), 0);
		Request->SetObjectField(TEXT("options"), Options);
		return SerializeObject(Request, OutJson);
	}

	TSharedPtr<FJsonObject> OperationResult(
		const FOperation& Operation,
		const FString& Status,
		const FString& Summary,
		const FString& Skipped = FString())
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("id"), Operation.Id);
		Result->SetStringField(TEXT("action"), Operation.Action);
		Result->SetStringField(TEXT("status"), Status);
		if (!Summary.IsEmpty())
		{
			Result->SetStringField(TEXT("summary"), Summary);
		}
		if (!Skipped.IsEmpty())
		{
			Result->SetStringField(TEXT("skipped_reason"), Skipped);
		}
		return Result;
	}

	void AppendNested(
		const TSharedPtr<FJsonObject>& Response,
		const FString& Field,
		TArray<TSharedPtr<FJsonValue>>& Out)
	{
		const TSharedPtr<FJsonObject>* Result = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Response.IsValid()
			&& Response->TryGetObjectField(TEXT("result"), Result)
			&& Result
			&& (*Result)->TryGetArrayField(Field, Values)
			&& Values)
		{
			Out.Append(*Values);
		}
	}
}

bool FUeremcpPlanExecutor::RegisterAction(
	const FString& Action,
	FUeremcpPlanOperationHandler&& Handler,
	FString& OutError)
{
	if (!ValidAction(Action) || Action == TEXT("execute_plan") || !Handler)
	{
		OutError = TEXT("plan action registration requires a valid non-recursive handler");
		return false;
	}
	FScopeLock Lock(&GPlanMutex);
	if (GHandlers.Contains(Action))
	{
		OutError = FString::Printf(TEXT("plan action '%s' is already registered"), *Action);
		return false;
	}
	GHandlers.Add(Action, MakeShared<FUeremcpPlanOperationHandler, ESPMode::ThreadSafe>(
		MoveTemp(Handler)));
	OutError.Reset();
	return true;
}

void FUeremcpPlanExecutor::UnregisterAction(const FString& Action)
{
	FScopeLock Lock(&GPlanMutex);
	GHandlers.Remove(Action);
}

void FUeremcpPlanExecutor::ClearActionHandlers()
{
	FScopeLock Lock(&GPlanMutex);
	GHandlers.Reset();
}

bool FUeremcpPlanExecutor::SetTransactionCallbacks(
	FUeremcpPlanTransactionCallbacks&& Callbacks,
	FString& OutError)
{
	if (!Callbacks.IsComplete())
	{
		OutError = TEXT("transaction callbacks require begin, commit, and rollback");
		return false;
	}
	FScopeLock Lock(&GPlanMutex);
	GTransaction = MakeShared<FUeremcpPlanTransactionCallbacks, ESPMode::ThreadSafe>(
		MoveTemp(Callbacks));
	OutError.Reset();
	return true;
}

void FUeremcpPlanExecutor::ClearTransactionCallbacks()
{
	FScopeLock Lock(&GPlanMutex);
	GTransaction.Reset();
}

bool FUeremcpPlanExecutor::ExecuteRequest(
	const FString& RequestJson,
	FString& OutResponseJson,
	FString& OutError)
{
	FUeremcpRequest Request;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, OutError))
	{
		return Reject(FString(), OutError, OutResponseJson, OutError);
	}
	if (Request.Action != TEXT("execute_plan"))
	{
		return Reject(Request.RequestId, TEXT("expected action execute_plan"), OutResponseJson, OutError);
	}
	TSharedPtr<FJsonObject> Root;
	if (!ParseObject(RequestJson, Root))
	{
		return Reject(Request.RequestId, TEXT("request is not a JSON object"), OutResponseJson, OutError);
	}

	TArray<FOperation> Operations;
	TArray<FString> Order;
	if (!ParseOperations(Request.Specification, Operations, Order, OutError))
	{
		return Reject(Request.RequestId, OutError, OutResponseJson, OutError);
	}
	TMap<FString, FOperation*> ById;
	for (FOperation& Operation : Operations)
	{
		ById.Add(Operation.Id, &Operation);
	}

	bool bAtomic = true;
	bool bRollbackOnFailure = true;
	FString CompilePolicy = TEXT("at_boundaries");
	FString ValidatePolicy = TEXT("at_end");
	const TSharedPtr<FJsonObject>* TransactionObject = nullptr;
	if (Request.Specification->TryGetObjectField(TEXT("transaction"), TransactionObject)
		&& TransactionObject && TransactionObject->IsValid())
	{
		(*TransactionObject)->TryGetBoolField(TEXT("atomic"), bAtomic);
		(*TransactionObject)->TryGetBoolField(TEXT("rollback_on_failure"), bRollbackOnFailure);
		(*TransactionObject)->TryGetStringField(TEXT("compile_policy"), CompilePolicy);
		(*TransactionObject)->TryGetStringField(TEXT("validate_policy"), ValidatePolicy);
	}
	FString OnFailure = TEXT("rollback_all");
	Request.Specification->TryGetStringField(TEXT("on_failure"), OnFailure);
	const TSet<FString> CompilePolicies = {
		TEXT("per_operation"), TEXT("at_boundaries"), TEXT("at_end"), TEXT("never")
	};
	const TSet<FString> ValidatePolicies = {
		TEXT("per_operation"), TEXT("at_boundaries"), TEXT("at_end")
	};
	const TSet<FString> FailurePolicies = {
		TEXT("stop"), TEXT("continue_independent"), TEXT("rollback_all")
	};
	if (!CompilePolicies.Contains(CompilePolicy)
		|| !ValidatePolicies.Contains(ValidatePolicy)
		|| !FailurePolicies.Contains(OnFailure))
	{
		return Reject(Request.RequestId, TEXT("invalid plan policy"), OutResponseJson, OutError);
	}

	TMap<FString, FHandlerPtr> Handlers;
	FTransactionPtr Transaction;
	{
		FScopeLock Lock(&GPlanMutex);
		Handlers = GHandlers;
		Transaction = GTransaction;
	}
	for (const FOperation& Operation : Operations)
	{
		if (!Handlers.Contains(Operation.Action))
		{
			return Reject(
				Request.RequestId,
				FString::Printf(TEXT("no handler registered for '%s'"), *Operation.Action),
				OutResponseJson,
				OutError);
		}
	}
	if (bAtomic && !Transaction.IsValid())
	{
		return Reject(
			Request.RequestId,
			TEXT("atomic execute_plan requires transaction callbacks"),
			OutResponseJson,
			OutError);
	}

	bool bTransactionOpen = false;
	if (bAtomic)
	{
		if (!Transaction->Begin(OutError))
		{
			return Reject(Request.RequestId, OutError, OutResponseJson, OutError);
		}
		bTransactionOpen = true;
	}

	TMap<FString, TSharedPtr<FJsonObject>> Completed;
	TMap<FString, FString> StatusById;
	TArray<TSharedPtr<FJsonValue>> Results;
	TArray<TSharedPtr<FJsonObject>> SuccessfulResponses;
	int32 InternalOperations = 0;
	bool bRequiredFailure = false;
	bool bAnyFailure = false;
	bool bAnyUsablePartial = false;
	bool bStop = false;

	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		FOperation& Operation = **ById.Find(Order[Index]);
		FString SkipReason;
		for (const FString& Dependency : Operation.DependsOn)
		{
			if (!Completed.Contains(Dependency))
			{
				SkipReason = FString::Printf(
					TEXT("dependency '%s' did not complete successfully"), *Dependency);
				break;
			}
		}
		if (bStop || !SkipReason.IsEmpty())
		{
			if (bStop)
			{
				SkipReason = TEXT("plan stopped after an earlier required failure");
			}
			Results.Add(MakeShared<FJsonValueObject>(OperationResult(
				Operation, TEXT("partially_completed"), TEXT("Operation was not executed."), SkipReason)));
			StatusById.Add(Operation.Id, TEXT("partially_completed"));
			bAnyFailure = true;
			bRequiredFailure |= !Operation.bOptional;
			continue;
		}

		bool bRun = true;
		if (!ConditionAllows(Operation, StatusById, bRun, SkipReason, OutError))
		{
			bRun = false;
			bAnyFailure = true;
		}
		if (!bRun)
		{
			Results.Add(MakeShared<FJsonValueObject>(OperationResult(
				Operation, TEXT("no_change_required"), TEXT("Condition not satisfied."), SkipReason)));
			StatusById.Add(Operation.Id, TEXT("no_change_required"));
			continue;
		}

		const bool bCompile = ShouldCompile(
			CompilePolicy, Operation, Operations, Index, Order.Num());
		const bool bValidate =
			ValidatePolicy == TEXT("per_operation")
			|| (ValidatePolicy == TEXT("at_boundaries") && bCompile)
			|| (ValidatePolicy == TEXT("at_end") && Index == Order.Num() - 1);
		FString NestedRequest;
		FString NestedResponse;
		FString Summary;
		FString Status = TEXT("error");
		TSharedPtr<FJsonObject> Response;
		if (!BuildOperationRequest(
			Root, Operation, Completed, bCompile, bValidate, NestedRequest, OutError)
			|| !(*Handlers.FindChecked(Operation.Action))(NestedRequest, NestedResponse, OutError)
			|| !ParseObject(NestedResponse, Response))
		{
			Summary = OutError.IsEmpty() ? TEXT("semantic handler failed") : OutError;
		}
		else
		{
			const TSharedPtr<FJsonObject>* Metrics = nullptr;
			double Count = 0.0;
			if (!Response->TryGetStringField(TEXT("status"), Status)
				|| !FUeremcpEnvelope::IsValidStatus(Status)
				|| !Response->TryGetStringField(TEXT("summary"), Summary)
				|| !Response->TryGetObjectField(TEXT("metrics"), Metrics)
				|| !Metrics
				|| !(*Metrics)->TryGetNumberField(TEXT("internal_operations"), Count))
			{
				Status = TEXT("error");
				Summary = TEXT("semantic handler returned an invalid response envelope");
			}
			else
			{
				InternalOperations += FMath::Max(0, static_cast<int32>(Count));
			}
		}

		Results.Add(MakeShared<FJsonValueObject>(OperationResult(Operation, Status, Summary)));
		StatusById.Add(Operation.Id, Status);
		const bool bUsablePartial = IsUsablePartial(Status, Response);
		if (IsSuccess(Status) || bUsablePartial)
		{
			Completed.Add(Operation.Id, Response);
			SuccessfulResponses.Add(Response);
			bAnyUsablePartial |= bUsablePartial;
		}
		else
		{
			bAnyFailure = true;
			bRequiredFailure |= !Operation.bOptional;
			bStop = !Operation.bOptional && OnFailure != TEXT("continue_independent");
		}
	}

	bool bRolledBack = false;
	if (bTransactionOpen)
	{
		if (bRequiredFailure && (bRollbackOnFailure || OnFailure == TEXT("rollback_all")))
		{
			bRolledBack = Transaction->Rollback(OutError);
		}
		else if (!Transaction->Commit(OutError))
		{
			bAnyFailure = true;
			bRequiredFailure = true;
			bRolledBack = Transaction->Rollback(OutError);
		}
	}

	FString FinalStatus;
	if (bRolledBack)
	{
		FinalStatus = TEXT("rolled_back");
	}
	else if (bAnyFailure && SuccessfulResponses.IsEmpty())
	{
		FinalStatus = TEXT("failed_validation");
	}
	else if (bAnyFailure || bAnyUsablePartial)
	{
		FinalStatus = TEXT("partially_completed");
	}
	else if (SuccessfulResponses.IsEmpty())
	{
		FinalStatus = TEXT("no_change_required");
	}
	else
	{
		SuccessfulResponses.Last()->TryGetStringField(TEXT("status"), FinalStatus);
	}
	TSharedPtr<FJsonObject> Final = (bRolledBack || SuccessfulResponses.IsEmpty())
		? BaseResponse(Request.RequestId, FinalStatus, TEXT("execute_plan produced no successful result."))
		: SuccessfulResponses.Last();
	Final->SetStringField(TEXT("protocol_version"), FUeremcpEnvelope::ProtocolVersion());
	Final->SetStringField(TEXT("request_id"), Request.RequestId);
	Final->SetStringField(TEXT("status"), FinalStatus);
	Final->SetStringField(
		TEXT("summary"),
		bRolledBack
			? TEXT("execute_plan failed and rolled back.")
			: bAnyFailure && SuccessfulResponses.IsEmpty()
				? TEXT("execute_plan failed before any operation completed successfully.")
			: bAnyFailure
				? TEXT("execute_plan completed only an independent subset.")
				: FString::Printf(TEXT("execute_plan completed %d operation(s)."), Operations.Num()));
	TSharedPtr<FJsonObject> Understood = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* ExistingUnderstood = nullptr;
	if (Final->TryGetObjectField(TEXT("understood"), ExistingUnderstood)
		&& ExistingUnderstood
		&& ExistingUnderstood->IsValid())
	{
		Understood = *ExistingUnderstood;
	}
	Understood->SetStringField(TEXT("action"), TEXT("execute_plan"));
	Final->SetObjectField(TEXT("understood"), Understood);
	TSharedPtr<FJsonObject> Aggregate = MakeShared<FJsonObject>();
	Aggregate->SetArrayField(TEXT("operations"), Results);
	if (!bRolledBack && !SuccessfulResponses.IsEmpty())
	{
		for (const FString& Field : {
			TEXT("created_assets"), TEXT("modified_assets"), TEXT("deleted_assets"),
			TEXT("reused_assets"), TEXT("dependencies"), TEXT("dependencies_created"),
			TEXT("unresolved_dependencies") })
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const TSharedPtr<FJsonObject>& Response : SuccessfulResponses)
			{
				AppendNested(Response, Field, Values);
			}
			if (!Values.IsEmpty())
			{
				Aggregate->SetArrayField(Field, Values);
			}
		}
		const TSharedPtr<FJsonObject>* TerminalResult = nullptr;
		FString PrimaryAsset;
		if (SuccessfulResponses.Last()->TryGetObjectField(TEXT("result"), TerminalResult)
			&& TerminalResult
			&& (*TerminalResult)->TryGetStringField(TEXT("primary_asset"), PrimaryAsset))
		{
			Aggregate->SetStringField(TEXT("primary_asset"), PrimaryAsset);
		}
	}
	Final->SetObjectField(TEXT("result"), Aggregate);
	TArray<TSharedPtr<FJsonValue>> Changes;
	if (!bRolledBack)
	{
		for (const TSharedPtr<FJsonObject>& Response : SuccessfulResponses)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (Response->TryGetArrayField(TEXT("changes"), Values) && Values)
			{
				Changes.Append(*Values);
			}
		}
	}
	if (!Changes.IsEmpty())
	{
		Final->SetArrayField(TEXT("changes"), Changes);
	}
	TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* ExistingMetrics = nullptr;
	if (Final->TryGetObjectField(TEXT("metrics"), ExistingMetrics)
		&& ExistingMetrics
		&& ExistingMetrics->IsValid())
	{
		Metrics = *ExistingMetrics;
	}
	Metrics->SetNumberField(TEXT("mcp_round_trips"), 1);
	Metrics->SetNumberField(TEXT("internal_operations"), InternalOperations);
	Final->SetObjectField(TEXT("metrics"), Metrics);
	if (bRolledBack)
	{
		TSharedPtr<FJsonObject> Rollback = MakeShared<FJsonObject>();
		Rollback->SetBoolField(TEXT("available"), true);
		Rollback->SetBoolField(TEXT("performed"), true);
		Rollback->SetStringField(TEXT("scope"), TEXT("full"));
		Final->SetObjectField(TEXT("rollback"), Rollback);
	}
	return Return(Final, OutResponseJson, OutError);
}
