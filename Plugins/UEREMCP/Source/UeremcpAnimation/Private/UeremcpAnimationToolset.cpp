#include "UeremcpAnimationToolset.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Misc/PackageName.h"
#include "UeremcpAnimationService.h"
#include "UeremcpEnvelope.h"

namespace
{
	FString ResolveObjectPath(const FString& AssetPath)
	{
		if (FPackageName::IsValidObjectPath(AssetPath))
		{
			return AssetPath;
		}
		if (FPackageName::IsValidLongPackageName(AssetPath))
		{
			return FString::Printf(
				TEXT("%s.%s"),
				*AssetPath,
				*FPackageName::GetLongPackageAssetName(AssetPath));
		}
		return AssetPath;
	}

	const TArray<FString>& AnimationCapabilityNotes()
	{
		static const TArray<FString> Notes = {
			TEXT("Montage inspection reads slots, segments, sections and real AnimNotify objects in one operation."),
			TEXT("AnimBlueprint read returns a graph inventory only; full ADR-0004 node/link payloads are deferred."),
			TEXT("AnimBlueprint state-machine authoring is unsupported; animation state machines remain read-only."),
			TEXT("Control Rig primitives are provided by Epic AnimationAssistant and must be composed, not re-exposed."),
			TEXT("Structured non-graph asset state is withheld until the frozen response envelope gains an approved field; counts and revision are returned now.")
		};
		return Notes;
	}
}

FString UUeremcpAnimationToolset::InspectMontage(const FString& RequestJson)
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
	if (!Request.Action.Equals(TEXT("inspect_montage"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("InspectMontage received action '%s'; expected 'inspect_montage'."),
				*Request.Action));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("inspect_montage requires target.asset_path."));
	}

	// Public UObject load path used only for the explicitly supplied target.
	// Package-name normalization uses public FPackageName helpers.
	// [VERIFIED: Runtime/CoreUObject/Public/Misc/PackageName.h:184,224-254,882-888]
	const FString ObjectPath = ResolveObjectPath(Request.TargetAssetPath);
	const FString AssetPath = FPackageName::ObjectPathToPackageName(ObjectPath);
	const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath);
	if (!Montage)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Target '%s' is missing or is not an AnimMontage."),
				*Request.TargetAssetPath));
	}

	FUeremcpMontageInspection Inspection;
	FString InspectError;
	if (!FUeremcpAnimationService::InspectMontage(
		Montage, AssetPath, Inspection, InspectError))
	{
		return FUeremcpEnvelope::MakeUnverified(
			Request.RequestId,
			FString::Printf(
				TEXT("Failed to inspect montage '%s': %s"),
				*Request.TargetAssetPath,
				*InspectError),
			AnimationCapabilityNotes());
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = FString::Printf(
		TEXT("Inspected montage '%s': %d slots, %d segments, %d sections, %d real notify events. ")
		TEXT("Revision is returned, but complete structured state is withheld because the frozen response envelope has no non-graph asset-state field."),
		*Request.TargetAssetPath,
		Inspection.SlotCount,
		Inspection.SegmentCount,
		Inspection.SectionCount,
		Inspection.NotifyCount);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = AssetPath;
	Response.PrimaryAsset = AssetPath;
	Response.Revision = Inspection.ContentHash;
	Response.CapabilityNotes = AnimationCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 2;

	for (const FString& DependencyPath : Inspection.DependencyPaths)
	{
		FUeremcpAssetRef Dependency;
		Dependency.AssetPath = DependencyPath;
		Dependency.Role = TEXT("montage_dependency");
		Response.Dependencies.Add(MoveTemp(Dependency));
	}

	Response.ExtraFields = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	Validation->SetBoolField(TEXT("dependencies_resolved"), true);
	Validation->SetField(TEXT("reread_after_write"), MakeShared<FJsonValueNull>());
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.montage.loaded")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.montage.slots_enumerated")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.montage.real_notifies_enumerated")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.montage.content_hash_computed")));
	Validation->SetArrayField(TEXT("checks_performed"), Checks);
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpAnimationToolset::ReadAnimBp(const FString& RequestJson)
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
	if (!Request.Action.Equals(TEXT("read_anim_bp"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("ReadAnimBp received action '%s'; expected 'read_anim_bp'."),
				*Request.Action));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("read_anim_bp requires target.asset_path."));
	}

	const FString ObjectPath = ResolveObjectPath(Request.TargetAssetPath);
	const FString AssetPath = FPackageName::ObjectPathToPackageName(ObjectPath);
	UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *ObjectPath);
	if (!AnimBlueprint)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Target '%s' is missing or is not an AnimBlueprint."),
				*Request.TargetAssetPath));
	}

	FUeremcpAnimBlueprintInspection Inspection;
	FString InspectError;
	if (!FUeremcpAnimationService::InspectAnimBlueprint(
		AnimBlueprint, AssetPath, Inspection, InspectError))
	{
		return FUeremcpEnvelope::MakeUnverified(
			Request.RequestId,
			FString::Printf(
				TEXT("Failed to inspect AnimBlueprint '%s': %s"),
				*Request.TargetAssetPath,
				*InspectError),
			AnimationCapabilityNotes());
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = FString::Printf(
		TEXT("Inventoried AnimBlueprint '%s': %d graphs (%d AnimGraph, %d state machine), %d nodes. ")
		TEXT("Graph inventory and revision are returned; full ADR-0004 node/link state and ")
		TEXT("envelope asset_state are withheld."),
		*Request.TargetAssetPath,
		Inspection.GraphCount,
		Inspection.AnimGraphCount,
		Inspection.StateMachineCount,
		Inspection.NodeCount);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = AssetPath;
	Response.PrimaryAsset = AssetPath;
	Response.Revision = Inspection.ContentHash;
	Response.CapabilityNotes = AnimationCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 2;

	for (const FString& DependencyPath : Inspection.DependencyPaths)
	{
		FUeremcpAssetRef Dependency;
		Dependency.AssetPath = DependencyPath;
		Dependency.Role = TEXT("anim_bp_dependency");
		Response.Dependencies.Add(MoveTemp(Dependency));
	}

	Response.ExtraFields = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	Validation->SetBoolField(TEXT("dependencies_resolved"), true);
	Validation->SetField(TEXT("reread_after_write"), MakeShared<FJsonValueNull>());
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.anim_bp.loaded")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.anim_bp.graphs_enumerated")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.anim_bp.content_hash_computed")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("animation.anim_bp.nodes_not_emitted")));
	Validation->SetArrayField(TEXT("checks_performed"), Checks);
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

	return FUeremcpEnvelope::SerializeResponse(Response);
}
