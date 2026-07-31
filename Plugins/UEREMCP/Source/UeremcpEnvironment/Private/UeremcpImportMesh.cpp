// UEREMCP — import_mesh_for_world (MCP-008).
//
// One call replacing import → unit-scale fight → collision preset → Nanite flag.
// Composes StaticMeshTools.import_file via ToolsetRegistry — does not reimplement
// FBX import.
//
// API NOTES — read, not recalled:
//   [VERIFIED: StaticMesh.h:2200/2204] GetBounds / GetBoundingBox
//   [VERIFIED: BodySetup.h] UBodySetup::CollisionTraceFlag / CTF_UseComplexAsSimple
//   [VERIFIED: StaticMesh.h NaniteSettings accessors]
//   [VERIFIED: UToolsetRegistry::ExecuteTool] sync via ToolCallAsyncResultString

#include "UeremcpEnvironmentToolset.h"

#include "UeremcpEnvelope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "BodySetupEnums.h" // PhysicsCore — not under PhysicsEngine/
#include "ObjectTools.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "HAL/PlatformProcess.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	bool CommonPreamble(
		const FString& RequestJson,
		const TCHAR* ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutRejection)
	{
		FString ParseError;
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, ParseError))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				FString(), FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
					*OutRequest.ProtocolVersion, *FUeremcpEnvelope::ProtocolVersion()));
			return false;
		}
		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("%s tool received action '%s'."),
					ExpectedAction, *OutRequest.Action));
			return false;
		}
		return true;
	}

	FString JsonObjectToString(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	bool CallStaticMeshImportFile(
		const FString& FolderPath,
		const FString& AssetName,
		const FString& SourceFile,
		FString& OutError)
	{
		if (!UToolsetRegistry::IsAvailable())
		{
			OutError = TEXT("ToolsetRegistry is not available");
			return false;
		}
		TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetStringField(TEXT("folder_path"), FolderPath);
		Args->SetStringField(TEXT("asset_name"), AssetName);
		Args->SetStringField(TEXT("source_file"), SourceFile);
		Args->SetBoolField(TEXT("import_materials"), true);
		Args->SetBoolField(TEXT("import_textures"), true);
		Args->SetBoolField(TEXT("combine_meshes"), true);

		UToolCallAsyncResultString* AsyncResult = UToolsetRegistry::ExecuteTool(
			TEXT("editor_toolset.toolsets.static_mesh.StaticMeshTools"),
			TEXT("import_file"),
			JsonObjectToString(Args.ToSharedRef()));
		if (!AsyncResult)
		{
			OutError = TEXT("StaticMeshTools.import_file returned null");
			return false;
		}
		const double Deadline = FPlatformTime::Seconds() + 120.0;
		while (!AsyncResult->bIsComplete && FPlatformTime::Seconds() < Deadline)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		if (!AsyncResult->bIsComplete)
		{
			OutError = TEXT("StaticMeshTools.import_file timed out");
			return false;
		}
		if (!AsyncResult->Error.IsEmpty())
		{
			OutError = AsyncResult->Error;
			return false;
		}
		return true;
	}

	void SplitAssetPath(const FString& AssetPath, FString& OutFolder, FString& OutName)
	{
		FString Path = AssetPath;
		if (Path.StartsWith(TEXT("/Game")))
		{
			// /Game/Foo/Bar/SM_X → folder /Game/Foo/Bar, name SM_X
		}
		int32 Slash = INDEX_NONE;
		Path.FindLastChar(TEXT('/'), Slash);
		if (Slash == INDEX_NONE)
		{
			OutFolder = TEXT("/Game");
			OutName = Path;
			return;
		}
		OutFolder = Path.Left(Slash);
		OutName = Path.Mid(Slash + 1);
		// Strip .SM_X object suffix if present
		int32 Dot = INDEX_NONE;
		if (OutName.FindChar(TEXT('.'), Dot))
		{
			OutName = OutName.Left(Dot);
		}
	}
}

FString UUeremcpEnvironmentToolset::ImportMeshForWorld(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("import_mesh_for_world"), Request, Rejection))
	{
		return Rejection;
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("import_mesh_for_world requires target.asset_path under /Game/."));
	}
	FString SourceFile;
	FString SourceUnit = TEXT("centimeters");
	FString Collision = TEXT("default");
	bool bNanite = false;
	TArray<double> ExpectedBoundsM;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("source_file"), SourceFile);
		Request.Specification->TryGetStringField(TEXT("source_unit"), SourceUnit);
		Request.Specification->TryGetStringField(TEXT("collision"), Collision);
		Request.Specification->TryGetBoolField(TEXT("nanite"), bNanite);
		const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("expected_bounds_m"), Bounds) && Bounds
			&& Bounds->Num() == 3)
		{
			ExpectedBoundsM = {
				(*Bounds)[0]->AsNumber(),
				(*Bounds)[1]->AsNumber(),
				(*Bounds)[2]->AsNumber()
			};
		}
	}
	if (SourceFile.IsEmpty() || !FPaths::FileExists(SourceFile))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("import_mesh_for_world requires specification.source_file pointing at an "
				 "existing file on disk."));
	}

	FString Folder, AssetName;
	SplitAssetPath(Request.TargetAssetPath, Folder, AssetName);

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would import %s → %s/%s via StaticMeshTools.import_file."),
			*SourceFile, *Folder, *AssetName);
		Response.Metrics.McpRoundTrips = 1;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString ImportError;
	if (!CallStaticMeshImportFile(Folder, AssetName, SourceFile, ImportError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("StaticMeshTools.import_file failed: %s"), *ImportError));
	}

	const FString ObjectPath = Folder / AssetName + TEXT(".") + AssetName;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
	if (!Mesh)
	{
		// Soft fallback: AssetRegistry lookup by package
		const FString PackageName = Folder / AssetName;
		Mesh = LoadObject<UStaticMesh>(nullptr, *(PackageName + TEXT(".") + AssetName));
	}
	if (!Mesh)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Import reported success but could not load StaticMesh at %s"),
				*ObjectPath));
	}

	// Collision. [VERIFIED: BodySetup.h CollisionTraceFlag]
	if (Collision.Equals(TEXT("complex_as_simple"), ESearchCase::IgnoreCase))
	{
		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			Body->Modify();
			Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
		}
	}

	// Nanite.
	{
		FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
		Nanite.bEnabled = bNanite;
		Mesh->SetNaniteSettings(Nanite);
		Mesh->NotifyNaniteSettingsChanged();
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector ExtentCm = Bounds.GetSize();
	const FVector ExtentM = ExtentCm / 100.f;

	TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ActualM;
	ActualM.Add(MakeShared<FJsonValueNumber>(ExtentM.X));
	ActualM.Add(MakeShared<FJsonValueNumber>(ExtentM.Y));
	ActualM.Add(MakeShared<FJsonValueNumber>(ExtentM.Z));
	Metrics->SetArrayField(TEXT("actual_bounds_m"), ActualM);
	Metrics->SetStringField(TEXT("source_unit"), SourceUnit);
	Metrics->SetStringField(TEXT("collision"), Collision);
	Metrics->SetBoolField(TEXT("nanite"), bNanite);

	if (ExpectedBoundsM.Num() == 3)
	{
		TArray<TSharedPtr<FJsonValue>> ExpectedJson;
		for (double V : ExpectedBoundsM) ExpectedJson.Add(MakeShared<FJsonValueNumber>(V));
		Metrics->SetArrayField(TEXT("expected_bounds_m"), ExpectedJson);

		const FVector Expected(
			ExpectedBoundsM[0], ExpectedBoundsM[1], ExpectedBoundsM[2]);
		const FVector Ratio(
			Expected.X > KINDA_SMALL_NUMBER ? ExtentM.X / Expected.X : 0.f,
			Expected.Y > KINDA_SMALL_NUMBER ? ExtentM.Y / Expected.Y : 0.f,
			Expected.Z > KINDA_SMALL_NUMBER ? ExtentM.Z / Expected.Z : 0.f);
		const bool bOk =
			FMath::Abs(Ratio.X - 1.f) <= 0.20f
			&& FMath::Abs(Ratio.Y - 1.f) <= 0.20f
			&& FMath::Abs(Ratio.Z - 1.f) <= 0.20f;
		if (!bOk)
		{
			// Reject after import leaves a silently unusable asset unless we delete it.
			TArray<UObject*> ToDelete;
			ToDelete.Add(Mesh);
			ObjectTools::DeleteObjectsUnchecked(ToDelete);

			TSharedPtr<FJsonObject> NextArgs = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
			Spec->SetStringField(
				TEXT("hint"),
				TEXT("Re-export with correct units (UE wants cm) or omit expected_bounds_m after verifying the numbers below. Rejected mesh asset was deleted."));
			NextArgs->SetObjectField(TEXT("specification"), Spec);
			FUeremcpResponse Response;
			Response.RequestId = Request.RequestId;
			Response.UnderstoodAction = Request.Action;
			Response.Status = TEXT("rejected");
			Response.ErrorCode = TEXT("MESH_BOUNDS_MISMATCH");
			Response.Summary = FString::Printf(
				TEXT("Imported bounds_m [%.2f, %.2f, %.2f] differ from expected_bounds_m "
					 "[%.2f, %.2f, %.2f] by more than 20%%. Refusing a silently unusable mesh "
					 "(deleted the imported asset)."),
				ExtentM.X, ExtentM.Y, ExtentM.Z,
				Expected.X, Expected.Y, Expected.Z);
			Response.NextArgs = NextArgs;
			Response.Metrics.McpRoundTrips = 1;
			Response.ExtraFields = MakeShared<FJsonObject>();
			Response.ExtraFields->SetObjectField(TEXT("structural_metrics"), Metrics);
			Response.CapabilityNotes.Add(
				TEXT("MESH_BOUNDS_MISMATCH deletes the just-imported asset so a bad FBX does "
					 "not linger under target.asset_path."));
			return FUeremcpEnvelope::SerializeResponse(Response);
		}
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Imported %s via StaticMeshTools.import_file; collision=%s nanite=%s."),
		*Mesh->GetPathName(), *Collision, bNanite ? TEXT("true") : TEXT("false"));
	Response.PrimaryAsset = Mesh->GetPathName();
	FUeremcpAssetRef Ref;
	Ref.AssetPath = Mesh->GetPathName();
	Ref.AssetClass = TEXT("StaticMesh");
	Response.CreatedAssets.Add(Ref);
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.AssetsAffected = 1;
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("structural_metrics"), Metrics);
	Response.CapabilityNotes.Add(
		TEXT("expected_bounds_m omitted → actual bounds reported so the agent can check. "
			 "A castle the size of a crate looks fine in the outliner."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}
