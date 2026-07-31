// UEREMCP — Environment toolset AICallable entry points (WS-01 / COVERAGE_PLAN III).

#include "UeremcpEnvironmentToolset.h"

#include "UeremcpEnvironmentService.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMutatingDispatch.h"

namespace
{
	void ApplyBuildResult(FUeremcpResponse& Response, const FUeremcpEnvironmentBuildResult& Result)
	{
		Response.Status = Result.Status;
		Response.Summary = Result.Summary;
		Response.CapabilityNotes = Result.CapabilityNotes;
		Response.Revision = Result.Revision;
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
		if (Result.bApproximated)
		{
			Extra->SetBoolField(TEXT("approximated"), true);
		}
		Response.ExtraFields = Extra;
	}

	enum class EEnvStage : uint8
	{
		Full,
		Landscape,
		Water,
		Foliage,
		Weather,
		Structures
	};

	void ApplyStageIncludes(FUeremcpEnvironmentBuildSpec& Spec, EEnvStage Stage)
	{
		Spec.bIncludeTerrain = false;
		Spec.bIncludeRiver = false;
		Spec.bIncludeForest = false;
		Spec.bIncludeRain = false;
		Spec.bIncludeLighting = false;
		Spec.bCaptureScreenshot = false;
		Spec.bIncludeStructures = false;
		switch (Stage)
		{
		case EEnvStage::Full:
			Spec.bIncludeTerrain = true;
			Spec.bIncludeRiver = true;
			Spec.bIncludeForest = true;
			Spec.bIncludeRain = true;
			Spec.bIncludeLighting = true;
			Spec.bCaptureScreenshot = false;
			break;
		case EEnvStage::Landscape:
			Spec.bIncludeTerrain = true;
			break;
		case EEnvStage::Water:
			Spec.bIncludeRiver = true;
			break;
		case EEnvStage::Foliage:
			Spec.bIncludeForest = true;
			break;
		case EEnvStage::Weather:
			Spec.bIncludeRain = true;
			Spec.bIncludeLighting = true;
			break;
		case EEnvStage::Structures:
			Spec.bIncludeStructures = true;
			break;
		}
	}

	FString DispatchEnvAction(
		const FString& RequestJson,
		const FString& ExpectedAction,
		EEnvStage Stage)
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
		if (!Request.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(
					TEXT("%s received action '%s'. Use action '%s'."),
					*ExpectedAction, *Request.Action, *ExpectedAction));
		}

		const bool bReadOnly = ExpectedAction.Equals(TEXT("inspect_environment"))
			|| ExpectedAction.Equals(TEXT("validate_environment"));
		FUeremcpEnvironmentBuildSpec Spec;
		FString SpecError;
		if (!bReadOnly
			&& !FUeremcpEnvironmentService::ParseBuildSpec(Request.Specification, Spec, SpecError))
		{
			return FUeremcpEnvelope::MakeRejection(Request.RequestId, SpecError);
		}
		if (Spec.DestinationLevelPath.IsEmpty())
		{
			Spec.DestinationLevelPath = Request.TargetAssetPath;
		}
		if (Stage != EEnvStage::Full)
		{
			ApplyStageIncludes(Spec, Stage);
		}

		FUeremcpMutatingDispatch Dispatch;
		FString Blocked;
		if (!Dispatch.TryBegin(RequestJson, bReadOnly, 0, bReadOnly, Blocked))
		{
			return Blocked;
		}

		FUeremcpEnvironmentBuildResult Result;
		if (ExpectedAction.Equals(TEXT("inspect_environment")))
		{
			Result = FUeremcpEnvironmentService::Inspect(Dispatch.GetRequest().TargetAssetPath);
		}
		else if (ExpectedAction.Equals(TEXT("validate_environment")))
		{
			Result = FUeremcpEnvironmentService::Validate(
				Dispatch.GetRequest().TargetAssetPath,
				Dispatch.GetRequest().Specification);
		}
		else
		{
			Result = FUeremcpEnvironmentService::Build(
				Dispatch.GetRequest(), Spec, Dispatch.IsEffectiveDryRun());
		}

		FUeremcpResponse Response;
		Response.RequestId = Dispatch.GetRequest().RequestId;
		Response.UnderstoodAction = Dispatch.GetRequest().Action;
		Response.UnderstoodTarget = Spec.DestinationLevelPath.IsEmpty()
			? Dispatch.GetRequest().TargetAssetPath
			: Spec.DestinationLevelPath;
		ApplyBuildResult(Response, Result);
		return Dispatch.Complete(Response);
	}
}

FString UUeremcpEnvironmentToolset::BuildEnvironment(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("build_environment"), EEnvStage::Full);
}

FString UUeremcpEnvironmentToolset::CreateLandscape(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("create_landscape"), EEnvStage::Landscape);
}

FString UUeremcpEnvironmentToolset::CreateWaterBody(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("create_water_body"), EEnvStage::Water);
}

FString UUeremcpEnvironmentToolset::ScatterFoliage(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("scatter_foliage"), EEnvStage::Foliage);
}

FString UUeremcpEnvironmentToolset::AttachWeather(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("attach_weather"), EEnvStage::Weather);
}

FString UUeremcpEnvironmentToolset::PlaceStructures(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("place_structures"), EEnvStage::Structures);
}

FString UUeremcpEnvironmentToolset::InspectEnvironment(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("inspect_environment"), EEnvStage::Full);
}

FString UUeremcpEnvironmentToolset::ValidateEnvironment(const FString& RequestJson)
{
	return DispatchEnvAction(RequestJson, TEXT("validate_environment"), EEnvStage::Full);
}
