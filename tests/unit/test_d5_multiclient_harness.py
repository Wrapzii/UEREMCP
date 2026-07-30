#!/usr/bin/env python3
"""Contract tests for the genuine D5 multi-client PIE harness."""

from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


class D5MultiClientHarnessTest(unittest.TestCase):
    def test_cpp_proof_requires_authority_replication_effect_and_damage(self):
        source = (
            REPO_ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpValidation"
            / "Private"
            / "Tests"
            / "GameplayPatternBMultiClient.spec.cpp"
        ).read_text(encoding="utf-8")

        for token in (
            "UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE",
            "PIE_ListenServer",
            "RemoteClientCount = 2",
            "SetPlayNumberOfClients(RemoteClientCount + 1)",
            "SetRunUnderOneProcess(true)",
            "BP_REGameMode.BP_REGameMode_C",
            "CreateNewMap()",
            "GameModeOverride",
            'AbilityId[] = TEXT("fire_s")',
            "ClientCaster->VisualCombatComponent->CastAbility",
            "ServerCaster->HasAuthority()",
            "ObserverCasterReplica->IsLocallyControlled()",
            "server_authority_accepted",
            "owner_observed_replicated_stamina",
            "second_client_observed_cast_effect",
            "server_applied_damage",
            "second_client_observed_replicated_damage",
            "CollectExpectedEffectPaths",
            "CountExpectedEffects",
            "IsObserverCastMontagePlaying",
            "Montage_IsPlaying",
            "VFXDefinition.LoadSynchronous()",
            "GetLongPackageName()",
            "GetAsset()",
            "bEnableTickRegen = false",
            "ECC_Visibility",
            "MinServerStamina",
            "bObserverSawCastMontage",
            "GetStamina()",
            "GetHealth()",
            "UEREMCP_D5_EVIDENCE",
            "UEREMCP_D5_OUTCOME=",
            "RequestEndPlayMap",
        ):
            self.assertIn(token, source)

        self.assertNotIn("ReadPixels", source)
        self.assertNotIn("TakeScreenshot", source)

    def test_runner_requires_machine_evidence(self):
        runner = (REPO_ROOT / "tests" / "run_d5_multiclient.ps1").read_text(
            encoding="utf-8"
        )
        for token in (
            "UnrealEditor-Cmd.exe",
            "UEREMCP.Validation.Gameplay.PatternB.MultiClientPIE",
            "UeremcpD5Output",
            "UEREMCP_D5_OUTCOME",
            "UEREMCP_D5_RUNNER",
            "ConvertFrom-Json",
            "WithRendering",
            "server_authority_accepted",
            "second_client_observed_cast_effect",
            "server_applied_damage",
            "second_client_observed_replicated_damage",
        ):
            self.assertIn(token, runner)


if __name__ == "__main__":
    unittest.main()
