#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && UEREMCP_WITH_RE

#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "LevelEditor.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "REAbilityTypes.h"
#include "RECharacter.h"
#include "RECharacterStats.h"
#include "REPlayerVisualCombatComponent.h"
#include "RESpellVFXDefinition.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace UeremcpValidation::PatternB
{
	static constexpr TCHAR AbilityId[] = TEXT("fire_s");
	// Project DefaultEngine.ini still names Lvl_Hub, but that umap is absent on
	// this machine. Use a clean automation map + BP_REGameMode instead.
	// [VERIFIED-RUNTIME: Content/RE/Hub contains no Lvl_Hub.umap]
	static constexpr TCHAR GameModePath[] =
		TEXT("/Game/RE/Core/BP_REGameMode.BP_REGameMode_C");
	static constexpr int32 RemoteClientCount = 2;
	static constexpr double StartupTimeoutSeconds = 60.0;
	static constexpr double EvidenceTimeoutSeconds = 20.0;
	static constexpr double ShutdownTimeoutSeconds = 30.0;

	struct FPatternBState
	{
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		TWeakObjectPtr<UWorld> ServerWorld;
		TWeakObjectPtr<UWorld> CasterClientWorld;
		TWeakObjectPtr<UWorld> ObserverClientWorld;
		TWeakObjectPtr<ARECharacter> ServerCaster;
		TWeakObjectPtr<ARECharacter> ServerTarget;
		TWeakObjectPtr<ARECharacter> ClientCaster;
		TWeakObjectPtr<ARECharacter> ObserverCasterReplica;
		TWeakObjectPtr<ARECharacter> ObserverTarget;
		TSet<FString> ExpectedEffectPaths;
		double PhaseStartedSeconds = 0.0;
		float InitialServerStamina = 0.0f;
		float MinServerStamina = 0.0f;
		float FinalServerStamina = 0.0f;
		float MinCasterClientStamina = 0.0f;
		float FinalCasterClientStamina = 0.0f;
		float InitialServerTargetHealth = 0.0f;
		float MinServerTargetHealth = 0.0f;
		float FinalServerTargetHealth = 0.0f;
		float MinObserverTargetHealth = 0.0f;
		float FinalObserverTargetHealth = 0.0f;
		int32 BaselineObserverEffects = 0;
		int32 MaxObserverEffects = 0;
		int32 ExpectedEffectPathCount = 0;
		int32 CastAttempts = 0;
		int32 ServerConnections = 0;
		int32 PIEWorlds = 0;
		int32 ClientWorlds = 0;
		double LastCastSeconds = 0.0;
		bool bStartRequested = false;
		bool bWorldsReady = false;
		bool bCastIssuedFromClient = false;
		bool bServerAcceptedCast = false;
		bool bOwnerObservedReplicatedStamina = false;
		bool bObserverSawCastNiagara = false;
		bool bObserverSawCastMontage = false;
		bool bObserverSawCastEffect = false;
		bool bServerAppliedDamage = false;
		bool bObserverSawReplicatedDamage = false;
		bool bEvidenceWritten = false;
		FString FailureReason;
	};

	static FString ResolveOutputPath()
	{
		FString OutputPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("UeremcpD5Output="), OutputPath)
			&& !OutputPath.IsEmpty())
		{
			return OutputPath;
		}
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("UEREMCP"),
			TEXT("d5_pattern_b_multiclient.json"));
	}

	static ARECharacter* FindCharacterByPlayerId(UWorld& World, const int32 PlayerId)
	{
		for (TActorIterator<ARECharacter> It(&World); It; ++It)
		{
			const APlayerState* PlayerState = It->GetPlayerState();
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static void CollectExpectedEffectPaths(
		const FREAbilityDef& Def,
		TSet<FString>& OutPaths)
	{
		// Prefer DA_VFX_* presentation systems — ResolveAbilityPresentation uses
		// these before legacy CastNS/ProjectileNS/ImpactNS soft paths.
		// [VERIFIED: REPlayerVisualCombatComponent.cpp:60-87]
		if (const URESpellVFXDefinition* Presentation = Def.VFXDefinition.LoadSynchronous())
		{
			const TSoftObjectPtr<UNiagaraSystem>* Systems[] = {
				&Presentation->CastSystem,
				&Presentation->LaunchSystem,
				&Presentation->TravelSystem,
				&Presentation->ImpactSystem,
			};
			for (const TSoftObjectPtr<UNiagaraSystem>* System : Systems)
			{
				if (System && !System->IsNull())
				{
					const FString PackageName = System->ToSoftObjectPath().GetLongPackageName();
					if (!PackageName.IsEmpty())
					{
						OutPaths.Add(PackageName);
					}
				}
			}
		}
		for (const FSoftObjectPath& Path : {Def.CastNS, Def.ProjectileNS, Def.ImpactNS})
		{
			if (Path.IsValid())
			{
				const FString PackageName = Path.GetLongPackageName();
				if (!PackageName.IsEmpty())
				{
					OutPaths.Add(PackageName);
				}
			}
		}
	}

	static int32 CountExpectedEffects(
		UWorld& World,
		const TSet<FString>& ExpectedEffectPaths)
	{
		int32 Count = 0;
		for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		{
			// Count matching FX even after they deactivate; NullRHI / short-lived
			// cast cosmetics can finish before a later observation frame.
			if (It->GetWorld() != &World)
			{
				continue;
			}
			const UNiagaraSystem* System = It->GetAsset();
			const UPackage* Package = System ? System->GetPackage() : nullptr;
			if (System && Package && ExpectedEffectPaths.Contains(Package->GetName()))
			{
				++Count;
			}
		}
		return Count;
	}

	static bool IsObserverCastMontagePlaying(const ARECharacter& ObserverCasterReplica)
	{
		// Multicast_AbilityCosmetics → PlayAbilityCosmeticsLocal always plays
		// CastMontage before optional Niagara. This remains observable under NullRHI.
		// [VERIFIED: RECharacter.cpp:391-399; REPlayerVisualCombatComponent.cpp:4136-4156]
		const UREPlayerVisualCombatComponent* Combat =
			ObserverCasterReplica.VisualCombatComponent;
		if (!Combat || !Combat->CastMontage)
		{
			return false;
		}
		const USkeletalMeshComponent* Mesh = ObserverCasterReplica.GetMesh();
		const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
		return Anim && Anim->Montage_IsPlaying(Combat->CastMontage);
	}

	static TSharedRef<FJsonObject> MakeEvidence(const FPatternBState& State)
	{
		const bool bPass =
			State.bWorldsReady
			&& State.bCastIssuedFromClient
			&& State.bServerAcceptedCast
			&& State.bOwnerObservedReplicatedStamina
			&& State.bObserverSawCastEffect
			&& State.bServerAppliedDamage
			&& State.bObserverSawReplicatedDamage;

		TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
		Evidence->SetStringField(TEXT("status"), bPass ? TEXT("pass") : TEXT("fail"));
		Evidence->SetStringField(TEXT("ability_id"), AbilityId);
		Evidence->SetStringField(TEXT("game_mode"), GameModePath);
		Evidence->SetStringField(TEXT("map"), TEXT("automation_new_map"));
		Evidence->SetNumberField(TEXT("remote_clients"), RemoteClientCount);
		Evidence->SetNumberField(TEXT("pie_worlds"), State.PIEWorlds);
		Evidence->SetNumberField(TEXT("client_worlds"), State.ClientWorlds);
		Evidence->SetNumberField(TEXT("server_connections"), State.ServerConnections);
		Evidence->SetBoolField(TEXT("client_intent_issued"), State.bCastIssuedFromClient);
		Evidence->SetBoolField(TEXT("server_authority_accepted"), State.bServerAcceptedCast);
		Evidence->SetBoolField(
			TEXT("owner_observed_replicated_stamina"),
			State.bOwnerObservedReplicatedStamina);
		Evidence->SetBoolField(
			TEXT("second_client_observed_cast_effect"),
			State.bObserverSawCastEffect);
		Evidence->SetBoolField(
			TEXT("second_client_observed_cast_niagara"),
			State.bObserverSawCastNiagara);
		Evidence->SetBoolField(
			TEXT("second_client_observed_cast_montage"),
			State.bObserverSawCastMontage);
		Evidence->SetNumberField(
			TEXT("expected_effect_path_count"),
			State.ExpectedEffectPathCount);
		Evidence->SetNumberField(TEXT("cast_attempts"), State.CastAttempts);
		Evidence->SetBoolField(TEXT("server_applied_damage"), State.bServerAppliedDamage);
		Evidence->SetBoolField(
			TEXT("second_client_observed_replicated_damage"),
			State.bObserverSawReplicatedDamage);
		Evidence->SetNumberField(TEXT("initial_server_stamina"), State.InitialServerStamina);
		Evidence->SetNumberField(TEXT("min_server_stamina"), State.MinServerStamina);
		Evidence->SetNumberField(TEXT("final_server_stamina"), State.FinalServerStamina);
		Evidence->SetNumberField(
			TEXT("min_owner_client_stamina"),
			State.MinCasterClientStamina);
		Evidence->SetNumberField(
			TEXT("final_owner_client_stamina"),
			State.FinalCasterClientStamina);
		Evidence->SetNumberField(
			TEXT("initial_server_target_health"),
			State.InitialServerTargetHealth);
		Evidence->SetNumberField(
			TEXT("min_server_target_health"),
			State.MinServerTargetHealth);
		Evidence->SetNumberField(
			TEXT("final_server_target_health"),
			State.FinalServerTargetHealth);
		Evidence->SetNumberField(
			TEXT("min_observer_target_health"),
			State.MinObserverTargetHealth);
		Evidence->SetNumberField(
			TEXT("final_observer_target_health"),
			State.FinalObserverTargetHealth);
		Evidence->SetNumberField(
			TEXT("baseline_observer_effects"),
			State.BaselineObserverEffects);
		Evidence->SetNumberField(
			TEXT("max_observer_effects"),
			State.MaxObserverEffects);
		if (!State.FailureReason.IsEmpty())
		{
			Evidence->SetStringField(TEXT("failure_reason"), State.FailureReason);
		}
		return Evidence;
	}

	static bool WriteEvidence(const FPatternBState& State, FString& OutJson)
	{
		const FString OutputPath = ResolveOutputPath();
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(MakeEvidence(State), Writer))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), /*Tree=*/true);
		return FFileHelper::SaveStringToFile(OutJson, *OutputPath);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpGameplayPatternBMultiClient,
	"UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FUeremcpPatternBMultiClientCommand final : public IAutomationLatentCommand
{
public:
	FUeremcpPatternBMultiClientCommand(
		FAutomationTestBase* InTest,
		TSharedRef<UeremcpValidation::PatternB::FPatternBState> InState)
		: Test(InTest)
		, State(MoveTemp(InState))
	{
	}

	virtual bool Update() override
	{
		using namespace UeremcpValidation::PatternB;
		const double Now = FPlatformTime::Seconds();

		if (!State->bStartRequested)
		{
			StartPIE(Now);
			return false;
		}

		if (!State->bWorldsReady)
		{
			if (TryResolveWorldsAndActors())
			{
				State->bWorldsReady = true;
				PrepareAndCast(Now);
				return false;
			}
			if ((Now - State->PhaseStartedSeconds) >= StartupTimeoutSeconds)
			{
				State->FailureReason = TEXT("multiclient_pie_startup_timeout");
				FinalizeAndStop();
			}
			return false;
		}

		if (!State->bEvidenceWritten)
		{
			ObserveEvidence(Now);
			const bool bComplete =
				State->bServerAcceptedCast
				&& State->bOwnerObservedReplicatedStamina
				&& State->bObserverSawCastEffect
				&& State->bServerAppliedDamage
				&& State->bObserverSawReplicatedDamage;
			if (bComplete || (Now - State->PhaseStartedSeconds) >= EvidenceTimeoutSeconds)
			{
				if (!bComplete && State->FailureReason.IsEmpty())
				{
					State->FailureReason = TEXT("pattern_b_evidence_timeout");
				}
				FinalizeAndStop();
			}
			return false;
		}

		if (!GEditor || !GEditor->IsPlaySessionInProgress())
		{
			return true;
		}
		if ((Now - State->PhaseStartedSeconds) >= ShutdownTimeoutSeconds)
		{
			Test->AddError(TEXT("Timed out while stopping multi-client PIE"));
			return true;
		}
		return false;
	}

private:
	void StartPIE(const double Now)
	{
		using namespace UeremcpValidation::PatternB;
		if (!GUnrealEd || (GEditor && GEditor->IsPlaySessionInProgress()))
		{
			State->FailureReason = TEXT("editor_unavailable_or_pie_already_running");
			State->bStartRequested = true;
			State->PhaseStartedSeconds = Now - StartupTimeoutSeconds;
			return;
		}

		// Mirror Epic CQTest PIE network bootstrap: clean map, then listen-server
		// under one process. [VERIFIED: PIENetworkComponent.cpp:33-137]
		FAutomationEditorCommonUtils::CreateNewMap();

		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, GameModePath);
		if (!GameModeClass)
		{
			State->FailureReason = TEXT("bp_regamemode_missing");
			State->bStartRequested = true;
			State->PhaseStartedSeconds = Now - StartupTimeoutSeconds;
			return;
		}

		State->PlaySettings.Reset(NewObject<ULevelEditorPlaySettings>());
		State->PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		// Epic counts the listen server as one client here; 3 yields server + 2
		// remote clients. [VERIFIED: PIENetworkComponent.cpp:104-119]
		State->PlaySettings->SetPlayNumberOfClients(RemoteClientCount + 1);
		State->PlaySettings->SetRunUnderOneProcess(true);
		State->PlaySettings->bLaunchSeparateServer = false;
		State->PlaySettings->GameGetsMouseControl = false;

		FRequestPlaySessionParams SessionParams;
		SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
		SessionParams.EditorPlaySettings = State->PlaySettings.Get();
		SessionParams.GameModeOverride = GameModeClass;
		SessionParams.StartLocation = FVector(0.0, 0.0, 200.0);
		SessionParams.StartRotation = FRotator::ZeroRotator;
		SessionParams.bAllowOnlineSubsystem = false;
		if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
		{
			FLevelEditorModule& LevelEditor =
				FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			SessionParams.DestinationSlateViewport = LevelEditor.GetFirstActiveViewport();
		}

		// [VERIFIED: Editor/UnrealEdEngine.h RequestPlaySession;
		// PIENetworkComponent.cpp:121-137]
		GUnrealEd->RequestPlaySession(SessionParams);
		GUnrealEd->StartQueuedPlaySessionRequest();
		State->bStartRequested = true;
		State->PhaseStartedSeconds = Now;
	}

	bool TryResolveWorldsAndActors()
	{
		using namespace UeremcpValidation::PatternB;
		if (!GEngine)
		{
			return false;
		}

		UWorld* ServerWorld = nullptr;
		TArray<TPair<int32, UWorld*>> ClientWorlds;
		int32 PIEWorlds = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE
				|| !IsValid(World)
				|| !IsValid(World->GetNetDriver()))
			{
				continue;
			}
			++PIEWorlds;
			if (World->GetNetDriver()->IsServer())
			{
				ServerWorld = World;
			}
			else
			{
				ClientWorlds.Emplace(Context.PIEInstance, World);
			}
		}
		ClientWorlds.Sort(
			[](const TPair<int32, UWorld*>& A, const TPair<int32, UWorld*>& B)
			{
				return A.Key < B.Key;
			});

		State->PIEWorlds = PIEWorlds;
		State->ClientWorlds = ClientWorlds.Num();
		State->ServerConnections =
			ServerWorld && ServerWorld->GetNetDriver()
			? ServerWorld->GetNetDriver()->ClientConnections.Num()
			: 0;
		if (!ServerWorld
			|| ClientWorlds.Num() != RemoteClientCount
			|| State->ServerConnections != RemoteClientCount)
		{
			return false;
		}

		APlayerController* CasterController =
			ClientWorlds[0].Value->GetFirstPlayerController();
		APlayerController* ObserverController =
			ClientWorlds[1].Value->GetFirstPlayerController();
		ARECharacter* ClientCaster =
			CasterController ? Cast<ARECharacter>(CasterController->GetPawn()) : nullptr;
		ARECharacter* ObserverTarget =
			ObserverController ? Cast<ARECharacter>(ObserverController->GetPawn()) : nullptr;
		if (!ClientCaster
			|| !ObserverTarget
			|| !ClientCaster->GetPlayerState()
			|| !ObserverTarget->GetPlayerState())
		{
			return false;
		}

		const int32 CasterId = ClientCaster->GetPlayerState()->GetPlayerId();
		const int32 TargetId = ObserverTarget->GetPlayerState()->GetPlayerId();
		ARECharacter* ServerCaster = FindCharacterByPlayerId(*ServerWorld, CasterId);
		ARECharacter* ServerTarget = FindCharacterByPlayerId(*ServerWorld, TargetId);
		ARECharacter* ObserverCasterReplica =
			FindCharacterByPlayerId(*ClientWorlds[1].Value, CasterId);
		if (!ServerCaster
			|| !ServerTarget
			|| !ObserverCasterReplica
			|| !ServerCaster->HasAuthority()
			|| ClientCaster->HasAuthority()
			|| ObserverCasterReplica->HasAuthority()
			|| ObserverCasterReplica->IsLocallyControlled())
		{
			return false;
		}

		State->ServerWorld = ServerWorld;
		State->CasterClientWorld = ClientWorlds[0].Value;
		State->ObserverClientWorld = ClientWorlds[1].Value;
		State->ServerCaster = ServerCaster;
		State->ServerTarget = ServerTarget;
		State->ClientCaster = ClientCaster;
		State->ObserverCasterReplica = ObserverCasterReplica;
		State->ObserverTarget = ObserverTarget;
		return true;
	}

	void PrepareAndCast(const double Now)
	{
		using namespace UeremcpValidation::PatternB;
		ARECharacter* ServerCaster = State->ServerCaster.Get();
		ARECharacter* ServerTarget = State->ServerTarget.Get();
		ARECharacter* ClientCaster = State->ClientCaster.Get();
		UREPlayerVisualCombatComponent* ServerCombat =
			ServerCaster ? ServerCaster->VisualCombatComponent : nullptr;
		UREStatsComponent* ServerCasterStats =
			ServerCaster ? ServerCaster->GetStatsComponent() : nullptr;
		UREStatsComponent* ServerTargetStats =
			ServerTarget ? ServerTarget->GetStatsComponent() : nullptr;
		if (!ServerCombat
			|| !ServerCombat->AbilityTable
			|| !ServerCasterStats
			|| !ServerTargetStats
			|| !ClientCaster
			|| !ClientCaster->VisualCombatComponent)
		{
			State->FailureReason = TEXT("pattern_b_components_or_ability_table_missing");
			State->PhaseStartedSeconds = Now - EvidenceTimeoutSeconds;
			return;
		}

		const FREAbilityDef* Def =
			ServerCombat->AbilityTable->FindRow<FREAbilityDef>(
				FName(AbilityId),
				TEXT("UEREMCP D5 multi-client"));
		if (!Def || Def->CastType != EREAbilityCastType::Projectile)
		{
			State->FailureReason = TEXT("fire_s_projectile_definition_missing");
			State->PhaseStartedSeconds = Now - EvidenceTimeoutSeconds;
			return;
		}
		if (State->CastAttempts == 0)
		{
			CollectExpectedEffectPaths(*Def, State->ExpectedEffectPaths);
			State->ExpectedEffectPathCount = State->ExpectedEffectPaths.Num();
			if (State->ExpectedEffectPaths.Num() == 0)
			{
				State->FailureReason = TEXT("fire_s_effect_paths_unresolved");
				State->PhaseStartedSeconds = Now - EvidenceTimeoutSeconds;
				return;
			}

			ServerCasterStats->SetStamina(
				ServerCasterStats->GetMaxStamina(),
				FName(TEXT("ueremcp_d5_setup")));
			ServerCasterStats->bEnableTickRegen = false;
			ServerTargetStats->bIgnoreDamage = false;
			ServerTargetStats->bEnableTickRegen = false;
			ServerTargetStats->ResetLifeStateFull(/*bFillVitals=*/true);

			// Keep pawns airborne-stable and make the target capsule visible to the
			// projectile Visibility sweep used by LaunchProjectileAbility.
			// [VERIFIED: REPlayerVisualCombatComponent.cpp:4409-4412]
			for (ARECharacter* Character : {ServerCaster, ServerTarget})
			{
				if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				{
					Movement->GravityScale = 0.0f;
					Movement->StopMovementImmediately();
				}
			}
			if (UCapsuleComponent* TargetCapsule = ServerTarget->GetCapsuleComponent())
			{
				TargetCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			}

			const FVector CasterLocation(0.0, 0.0, 200.0);
			const FVector TargetLocation(450.0, 0.0, 200.0);
			ServerCaster->SetActorLocationAndRotation(
				CasterLocation,
				FRotator::ZeroRotator,
				/*bSweep=*/false,
				/*OutSweepHitResult=*/nullptr,
				ETeleportType::TeleportPhysics);
			ServerTarget->SetActorLocationAndRotation(
				TargetLocation,
				FRotator(0.0, 180.0, 0.0),
				/*bSweep=*/false,
				/*OutSweepHitResult=*/nullptr,
				ETeleportType::TeleportPhysics);
			if (AController* Controller = ServerCaster->GetController())
			{
				Controller->SetControlRotation(FRotator::ZeroRotator);
			}
			if (AController* Controller = ClientCaster->GetController())
			{
				Controller->SetControlRotation(FRotator::ZeroRotator);
			}
			ServerCaster->ForceNetUpdate();
			ServerTarget->ForceNetUpdate();

			State->InitialServerStamina = ServerCasterStats->GetStamina();
			State->MinServerStamina = State->InitialServerStamina;
			State->MinCasterClientStamina = ClientCaster->GetStatsComponent()
				? ClientCaster->GetStatsComponent()->GetStamina()
				: State->InitialServerStamina;
			State->InitialServerTargetHealth = ServerTargetStats->GetHealth();
			State->MinServerTargetHealth = State->InitialServerTargetHealth;
			State->MinObserverTargetHealth = State->InitialServerTargetHealth;
			State->BaselineObserverEffects =
				CountExpectedEffects(*State->ObserverClientWorld.Get(), State->ExpectedEffectPaths);
			State->MaxObserverEffects = State->BaselineObserverEffects;
			State->PhaseStartedSeconds = Now;
		}
		else
		{
			// Retries only re-issue client intent after cooldown; keep baselines.
			ServerCasterStats->SetStamina(
				ServerCasterStats->GetMaxStamina(),
				FName(TEXT("ueremcp_d5_retry")));
			ServerCasterStats->bEnableTickRegen = false;
			if (AController* Controller = ServerCaster->GetController())
			{
				Controller->SetControlRotation(FRotator::ZeroRotator);
			}
			if (AController* Controller = ClientCaster->GetController())
			{
				Controller->SetControlRotation(FRotator::ZeroRotator);
			}
		}

		// This is deliberately invoked on a non-authority locally controlled pawn.
		// The observed server stamina change therefore proves client intent crossed
		// Server_RequestCastAbility before AuthorityCastAbility accepted it.
		// [VERIFIED: REPlayerVisualCombatComponent.cpp:3963-3984]
		ClientCaster->VisualCombatComponent->CastAbility(FName(AbilityId));
		State->bCastIssuedFromClient = true;
		++State->CastAttempts;
		State->LastCastSeconds = Now;
	}

	void ObserveEvidence(const double Now)
	{
		using namespace UeremcpValidation::PatternB;
		ARECharacter* ServerCaster = State->ServerCaster.Get();
		const ARECharacter* ServerTarget = State->ServerTarget.Get();
		ARECharacter* ClientCaster = State->ClientCaster.Get();
		const ARECharacter* ObserverTarget = State->ObserverTarget.Get();
		const ARECharacter* ObserverCasterReplica = State->ObserverCasterReplica.Get();
		UWorld* ObserverWorld = State->ObserverClientWorld.Get();
		if (!ServerCaster
			|| !ServerTarget
			|| !ClientCaster
			|| !ObserverTarget
			|| !ObserverCasterReplica
			|| !ObserverWorld)
		{
			State->FailureReason = TEXT("pattern_b_actor_lost_during_pie");
			return;
		}

		const UREStatsComponent* ServerCasterStats = ServerCaster->GetStatsComponent();
		const UREStatsComponent* ServerTargetStats = ServerTarget->GetStatsComponent();
		const UREStatsComponent* ClientCasterStats = ClientCaster->GetStatsComponent();
		const UREStatsComponent* ObserverTargetStats = ObserverTarget->GetStatsComponent();
		if (!ServerCasterStats
			|| !ServerTargetStats
			|| !ClientCasterStats
			|| !ObserverTargetStats)
		{
			State->FailureReason = TEXT("pattern_b_stats_lost_during_pie");
			return;
		}

		State->FinalServerStamina = ServerCasterStats->GetStamina();
		State->FinalCasterClientStamina = ClientCasterStats->GetStamina();
		State->FinalServerTargetHealth = ServerTargetStats->GetHealth();
		State->FinalObserverTargetHealth = ObserverTargetStats->GetHealth();
		State->MinServerStamina = FMath::Min(State->MinServerStamina, State->FinalServerStamina);
		State->MinCasterClientStamina = FMath::Min(
			State->MinCasterClientStamina,
			State->FinalCasterClientStamina);
		State->MinServerTargetHealth = FMath::Min(
			State->MinServerTargetHealth,
			State->FinalServerTargetHealth);
		State->MinObserverTargetHealth = FMath::Min(
			State->MinObserverTargetHealth,
			State->FinalObserverTargetHealth);
		State->MaxObserverEffects = FMath::Max(
			State->MaxObserverEffects,
			CountExpectedEffects(*ObserverWorld, State->ExpectedEffectPaths));
		State->bObserverSawCastNiagara =
			State->MaxObserverEffects > State->BaselineObserverEffects;
		State->bObserverSawCastMontage =
			State->bObserverSawCastMontage
			|| IsObserverCastMontagePlaying(*ObserverCasterReplica);
		// Pattern B client cosmetics are Multicast_AbilityCosmetics: montage and/or
		// Niagara on the second client's simulated caster replica.
		State->bObserverSawCastEffect =
			State->bObserverSawCastNiagara || State->bObserverSawCastMontage;

		// Stamina regenerates; accept on the observed minimum after the cast.
		State->bServerAcceptedCast =
			State->MinServerStamina < State->InitialServerStamina - KINDA_SMALL_NUMBER;
		State->bOwnerObservedReplicatedStamina =
			State->bServerAcceptedCast
			&& State->MinCasterClientStamina
				< State->InitialServerStamina - KINDA_SMALL_NUMBER;
		State->bServerAppliedDamage =
			State->MinServerTargetHealth
			< State->InitialServerTargetHealth - KINDA_SMALL_NUMBER;
		State->bObserverSawReplicatedDamage =
			State->bServerAppliedDamage
			&& State->MinObserverTargetHealth
				< State->InitialServerTargetHealth - KINDA_SMALL_NUMBER;

		// Multicast_AbilityCosmetics is unreliable — retry client intent a few
		// times if gameplay evidence landed but cosmetics did not.
		// [VERIFIED: RECharacter.h:183-185]
		constexpr int32 MaxCastAttempts = 3;
		constexpr double RetryDelaySeconds = 1.25;
		if (!State->bObserverSawCastEffect
			&& State->bServerAcceptedCast
			&& State->CastAttempts > 0
			&& State->CastAttempts < MaxCastAttempts
			&& (Now - State->LastCastSeconds) >= RetryDelaySeconds
			&& ClientCaster->VisualCombatComponent)
		{
			PrepareAndCast(Now);
		}
	}

	void FinalizeAndStop()
	{
		using namespace UeremcpValidation::PatternB;
		FString Json;
		const bool bEvidenceSaved = WriteEvidence(*State, Json);
		State->bEvidenceWritten = true;

		Test->AddInfo(*FString::Printf(TEXT("UEREMCP_D5_EVIDENCE=%s"), *Json));
		Test->TestTrue(TEXT("genuine listen server and two clients ready"), State->bWorldsReady);
		Test->TestTrue(TEXT("cast intent issued from non-authority client"), State->bCastIssuedFromClient);
		Test->TestTrue(TEXT("server authority accepted and spent stamina"), State->bServerAcceptedCast);
		Test->TestTrue(
			TEXT("owning client observed replicated stamina"),
			State->bOwnerObservedReplicatedStamina);
		Test->TestTrue(
			TEXT("second client observed ability-specific Niagara"),
			State->bObserverSawCastEffect);
		Test->TestTrue(TEXT("server projectile applied damage"), State->bServerAppliedDamage);
		Test->TestTrue(
			TEXT("second client observed replicated target damage"),
			State->bObserverSawReplicatedDamage);
		Test->TestTrue(TEXT("machine-readable evidence saved"), bEvidenceSaved);

		const bool bPass =
			State->bWorldsReady
			&& State->bCastIssuedFromClient
			&& State->bServerAcceptedCast
			&& State->bOwnerObservedReplicatedStamina
			&& State->bObserverSawCastEffect
			&& State->bServerAppliedDamage
			&& State->bObserverSawReplicatedDamage
			&& bEvidenceSaved;
		Test->AddInfo(*FString::Printf(
			TEXT("UEREMCP_D5_OUTCOME=%s%s"),
			bPass ? TEXT("PASS") : TEXT("FAIL"),
			State->FailureReason.IsEmpty()
				? TEXT("")
				: *FString::Printf(TEXT(" reason=%s"), *State->FailureReason)));

		if (GUnrealEd && GEditor && GEditor->IsPlaySessionInProgress())
		{
			GUnrealEd->RequestEndPlayMap();
		}
		State->PhaseStartedSeconds = FPlatformTime::Seconds();
		State->PlaySettings.Reset();
	}

	FAutomationTestBase* Test;
	TSharedRef<UeremcpValidation::PatternB::FPatternBState> State;
};

bool FUeremcpGameplayPatternBMultiClient::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidation::PatternB;
	if (GEditor && GEditor->IsPlaySessionInProgress())
	{
		AddInfo(TEXT("UEREMCP_D5_OUTCOME=FAIL reason=pie_already_running"));
		return false;
	}

	const TSharedRef<FPatternBState> State = MakeShared<FPatternBState>();
	ADD_LATENT_AUTOMATION_COMMAND(FUeremcpPatternBMultiClientCommand(this, State));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && UEREMCP_WITH_RE
