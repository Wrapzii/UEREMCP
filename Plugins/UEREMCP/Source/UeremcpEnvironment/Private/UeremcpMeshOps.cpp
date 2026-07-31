// UEREMCP — submit_mesh_ops: author a StaticMesh from GeometryScript primitives.
//
// THE MESH PRIMITIVE FLOOR.
//
// Every asset-consuming action (scatter_foliage, place_structures) took a
// mesh_path as an input and substituted /Engine/BasicShapes/Cube when it was
// absent. From an empty project there is no mesh to pass, so the substitution
// was guaranteed: cubes for trees, every time.
//
// GeometryScript was already a dependency (UeremcpEnvironment.Build.cs:37) and
// AppendBox was already in use (UeremcpEnvironmentService.cpp:1244) — but welded
// inside place_structures, spawning ADynamicMeshActor into the world. Nothing
// authored a StaticMesh ASSET, which is what foliage scattering needs.
//
// This exposes that capability as an op document: primitives composed in order,
// same read/modify/apply shape as the Blueprint graph tools.
//
// COMPILE NOTES FOR REVIEW — read before building:
//   [VERIFIED: MeshPrimitiveFunctions.h:168] AppendBox
//   [UNVERIFIED] AppendCylinder / AppendCone / AppendSphere parameter lists were
//     NOT read. Signatures follow the AppendBox pattern but may differ in
//     argument order or count. Each is isolated in its own ApplyOp_* branch so a
//     mismatch is a local fix, not a rewrite.
//   [UNVERIFIED] CreateNewStaticMeshAssetFromMesh lives in GeometryScriptingEditor,
//     which is added to Build.cs by this change. If the symbol or its options
//     struct differs, only MakeStaticMeshAsset() below needs editing.

#include "UeremcpEnvironmentToolset.h"

#include "UeremcpEnvelope.h"

#include "UDynamicMesh.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"

#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"

namespace
{
	FVector VectorFromField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FVector& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr) && Arr && Arr->Num() == 3)
		{
			return FVector(
				(*Arr)[0]->AsNumber(),
				(*Arr)[1]->AsNumber(),
				(*Arr)[2]->AsNumber());
		}
		return Fallback;
	}

	double NumberOr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double Fallback)
	{
		double Out = Fallback;
		return (Obj.IsValid() && Obj->TryGetNumberField(Field, Out)) ? Out : Fallback;
	}

	/**
	 * Apply one op. Returns false with a reason for an unknown op rather than
	 * skipping it: a silently ignored op produces a mesh that is not what was
	 * asked for, which is the exact failure this action exists to end.
	 */
	bool ApplyOp(
		UDynamicMesh* Mesh,
		const TSharedPtr<FJsonObject>& Op,
		FString& OutError)
	{
		const FString Kind = Op->GetStringField(TEXT("op"));
		FGeometryScriptPrimitiveOptions Options;
		FTransform Where(VectorFromField(Op, TEXT("origin"), FVector::ZeroVector));

		if (Kind.Equals(TEXT("box"), ESearchCase::IgnoreCase))
		{
			const FVector Size = VectorFromField(Op, TEXT("size"), FVector(100.0, 100.0, 100.0));
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
				Mesh, Options, Where,
				Size.X, Size.Y, Size.Z,
				0, 0, 0,
				EGeometryScriptPrimitiveOriginMode::Base,
				nullptr);
			return true;
		}
		if (Kind.Equals(TEXT("cylinder"), ESearchCase::IgnoreCase))
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
				Mesh, Options, Where,
				NumberOr(Op, TEXT("radius"), 50.0),
				NumberOr(Op, TEXT("height"), 100.0),
				FMath::Max(3, (int32)NumberOr(Op, TEXT("radial_steps"), 12.0)),
				FMath::Max(1, (int32)NumberOr(Op, TEXT("height_steps"), 1.0)),
				/*bCapped=*/true,
				EGeometryScriptPrimitiveOriginMode::Base,
				nullptr);
			return true;
		}
		if (Kind.Equals(TEXT("cone"), ESearchCase::IgnoreCase))
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
				Mesh, Options, Where,
				NumberOr(Op, TEXT("base_radius"), 60.0),
				NumberOr(Op, TEXT("top_radius"), 0.0),
				NumberOr(Op, TEXT("height"), 150.0),
				FMath::Max(3, (int32)NumberOr(Op, TEXT("radial_steps"), 12.0)),
				FMath::Max(1, (int32)NumberOr(Op, TEXT("height_steps"), 1.0)),
				/*bCapped=*/true,
				EGeometryScriptPrimitiveOriginMode::Base,
				nullptr);
			return true;
		}
		if (Kind.Equals(TEXT("sphere"), ESearchCase::IgnoreCase))
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(
				Mesh, Options, Where,
				NumberOr(Op, TEXT("radius"), 50.0),
				FMath::Max(3, (int32)NumberOr(Op, TEXT("steps_phi"), 8.0)),
				FMath::Max(3, (int32)NumberOr(Op, TEXT("steps_theta"), 12.0)),
				EGeometryScriptPrimitiveOriginMode::Center,
				nullptr);
			return true;
		}

		OutError = FString::Printf(
			TEXT("unsupported op '%s'; supported: box, cylinder, cone, sphere"),
			*Kind);
		return false;
	}

	/** Isolated so a signature mismatch here is a one-function fix. [UNVERIFIED] */
	UStaticMesh* MakeStaticMeshAsset(UDynamicMesh* Mesh, const FString& AssetPath, FString& OutError)
	{
		FGeometryScriptCreateNewStaticMeshAssetOptions Options;
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;

		UStaticMesh* Created =
			UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
				Mesh, AssetPath, Options, Outcome, nullptr);

		if (Outcome != EGeometryScriptOutcomePins::Success || Created == nullptr)
		{
			OutError = FString::Printf(
				TEXT("CreateNewStaticMeshAssetFromMesh failed for %s"), *AssetPath);
			return nullptr;
		}
		return Created;
	}
}

FString UUeremcpEnvironmentToolset::SubmitMeshOps(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("submit_mesh_ops"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("submit_mesh_ops tool received action '%s'."), *Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("submit_mesh_ops requires target.asset_path (StaticMesh under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/)."));
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!Request.Specification.IsValid()
		|| !Request.Specification->TryGetArrayField(TEXT("ops"), OpsArray)
		|| OpsArray == nullptr
		|| OpsArray->Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("submit_mesh_ops requires a non-empty specification.ops array. "
				 "Each entry: {\"op\":\"box|cylinder|cone|sphere\", ...}."));
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = OpsArray->Num();

	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would author StaticMesh %s from %d op(s)."),
			*Request.TargetAssetPath,
			OpsArray->Num());
		Response.CapabilityNotes.Add(
			TEXT("Dry run does not build geometry; triangle counts are unknown until a real run."));
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	UDynamicMesh* Mesh = NewObject<UDynamicMesh>();
	if (Mesh == nullptr)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("Failed to allocate UDynamicMesh."));
	}

	int32 Applied = 0;
	for (const TSharedPtr<FJsonValue>& Value : *OpsArray)
	{
		const TSharedPtr<FJsonObject>* OpObj = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(OpObj) || !OpObj)
		{
			continue;
		}
		FString OpError;
		if (!ApplyOp(Mesh, *OpObj, OpError))
		{
			// Refuse the whole request. A partially-built mesh saved under the
			// requested name is indistinguishable from a correct one downstream.
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(TEXT("submit_mesh_ops op %d rejected: %s"), Applied, *OpError));
		}
		++Applied;
	}

	FString AssetError;
	UStaticMesh* Created = MakeStaticMeshAsset(Mesh, Request.TargetAssetPath, AssetError);
	if (Created == nullptr)
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = AssetError;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	// Never *_validated: the asset is written but not re-read, and nothing here
	// proves it renders (AGENTS.md rule 6).
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Authored StaticMesh %s from %d op(s)."), *Request.TargetAssetPath, Applied);
	Response.PrimaryAsset = Request.TargetAssetPath;

	FUeremcpAssetRef Ref;
	Ref.AssetPath = Request.TargetAssetPath;
	Ref.AssetClass = TEXT("StaticMesh");
	Response.CreatedAssets.Add(Ref);

	Response.Metrics.AssetsAffected = 1;
	Response.CapabilityNotes.Add(
		TEXT("Geometry written and saved; NOT structurally re-read and NOT rendered. "
			 "Status is not *_validated. Use CaptureWorldFrames to prove appearance."));
	Response.CapabilityNotes.Add(
		TEXT("No UV unwrap, LOD generation, or collision is performed. Supply those "
			 "separately if the mesh needs them."));

	return FUeremcpEnvelope::SerializeResponse(Response);
}
