// UEREMCP — Environment toolset AICallable entry points (WS-01).

#include "UeremcpEnvironmentToolset.h"

#include "UeremcpEnvironmentService.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpSecurityDomainAdoption.h"

namespace
{
	void ApplyBuildResult(FUeremcpResponse& Response, const FUeremcpEnvironmentBuildResult& Result)
	{
		Response.Status = Result.Status;
		Response.Summary = Result.Summary;
		Response.CapabilityNotes = Result.CapabilityNotes;
		for (const FString& W : Result.Warnings)
		{
			Response.CapabilityNotes.Add(FString::Printf(TEXT("warning: %s"), *W));
		}
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = Result.InternalOperations;

		TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
		if (Result.RealVsApproximated.IsValid())
		{
			Extra->SetObjectField(TEXT("real_vs_approximated"), Result.RealVsApproximated);
		}
		if (Result.StructuralMetrics.IsValid())
		{
			Extra->SetObjectField(TEXT("structural_metrics"), Result.StructuralMetrics);
		}
		if (Result.ChangeManifest.IsValid())
		{
			Extra->SetObjectField(TEXT("change_manifest"), Result.ChangeManifest);
		}
		if (Result.ScreenshotPaths.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Shots;
			for (const FString& P : Result.ScreenshotPaths)
			{
				Shots.Add(MakeShared<FJsonValueString>(P));
			}
			Extra->SetArrayField(TEXT("screenshots"), Shots);
		}
		if (Result.Warnings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Warns;
			for (const FString& W : Result.Warnings)
			{
				Warns.Add(MakeShared<FJsonValueString>(W));
			}
			Extra->SetArrayField(TEXT("warnings"), Warns);
		}
		Response.ExtraFields = Extra;
	}
}

FString UUeremcpEnvironmentToolset::BuildEnvironment(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion()));
	}
	if (!Request.Action.Equals(TEXT("build_environment"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("BuildEnvironment received action '%s'. Use action 'build_environment'."),
				*Request.Action));
	}

	FUeremcpEnvironmentBuildSpec Spec;
	FString SpecError;
	if (!FUeremcpEnvironmentService::ParseBuildSpec(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, SpecError);
	}
	if (Spec.DestinationLevelPath.IsEmpty())
	{
		Spec.DestinationLevelPath = Request.TargetAssetPath;
	}

	const bool bTargetExists = false;
	FUeremcpMutatingDispatch Dispatch;
	FString Blocked;
	if (!Dispatch.TryBegin(RequestJson, bTargetExists, 0, false, Blocked))
	{
		return Blocked;
	}

	const FUeremcpEnvironmentBuildResult Result = FUeremcpEnvironmentService::Build(
		Dispatch.GetRequest(),
		Spec,
		Dispatch.IsEffectiveDryRun());

	FUeremcpResponse Response;
	Response.RequestId = Dispatch.GetRequest().RequestId;
	Response.UnderstoodAction = Dispatch.GetRequest().Action;
	Response.UnderstoodTarget = Spec.DestinationLevelPath;
	ApplyBuildResult(Response, Result);
	return Dispatch.Complete(Response);
}

FString UUeremcpEnvironmentToolset::InspectEnvironment(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}
	if (!Request.Action.Equals(TEXT("inspect_environment"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("InspectEnvironment received action '%s'. Use action 'inspect_environment'."),
				*Request.Action));
	}

	FUeremcpMutatingDispatch Dispatch;
	FString Blocked;
	if (!Dispatch.TryBegin(RequestJson, true, 0, true, Blocked))
	{
		return Blocked;
	}

	const FUeremcpEnvironmentBuildResult Result =
		FUeremcpEnvironmentService::Inspect(Dispatch.GetRequest().TargetAssetPath);
	FUeremcpResponse Response;
	Response.RequestId = Dispatch.GetRequest().RequestId;
	Response.UnderstoodAction = Dispatch.GetRequest().Action;
	Response.UnderstoodTarget = Dispatch.GetRequest().TargetAssetPath;
	ApplyBuildResult(Response, Result);
	return Dispatch.Complete(Response);
}

FString UUeremcpEnvironmentToolset::ValidateEnvironment(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}
	if (!Request.Action.Equals(TEXT("validate_environment"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("ValidateEnvironment received action '%s'. Use action 'validate_environment'."),
				*Request.Action));
	}

	FUeremcpMutatingDispatch Dispatch;
	FString Blocked;
	if (!Dispatch.TryBegin(RequestJson, true, 0, true, Blocked))
	{
		return Blocked;
	}

	const FUeremcpEnvironmentBuildResult Result = FUeremcpEnvironmentService::Validate(
		Dispatch.GetRequest().TargetAssetPath,
		Dispatch.GetRequest().Specification);
	FUeremcpResponse Response;
	Response.RequestId = Dispatch.GetRequest().RequestId;
	Response.UnderstoodAction = Dispatch.GetRequest().Action;
	Response.UnderstoodTarget = Dispatch.GetRequest().TargetAssetPath;
	ApplyBuildResult(Response, Result);
	return Dispatch.Complete(Response);
}
