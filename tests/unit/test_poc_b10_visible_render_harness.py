#!/usr/bin/env python3
"""Contract tests for the POC B B10 visible-render harness."""

from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


class PocB10VisibleRenderHarnessTest(unittest.TestCase):
    def test_filter_requires_programmatic_pixels_and_supplementary_png(self):
        source = (
            REPO_ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpValidation"
            / "Private"
            / "Tests"
            / "NiagaraPocBVisibleRender.spec.cpp"
        ).read_text(encoding="utf-8")

        for token in (
            "UEREMCP.Niagara.POCB.VisibleRender",
            "/Game/__UeremcpPoc/NS_POCB_Fireball",
            "ReadPixels",
            "PNGCompressImageArray",
            "MinimumChangedPixels",
            "MinimumWarmChangedPixels",
            "MinimumWarmupFrames",
            "WarmupSeconds",
            "UeremcpPocB10System=",
            "IAutomationLatentCommand",
            "FUeremcpPocB10BaselineCommand",
            "FUeremcpPocB10WarmupCommand",
            "FUeremcpPocB10FinalizeCommand",
            "ADD_LATENT_AUTOMATION_COMMAND",
            "LEVELTICK_ViewportsOnly",
            "EngineShowFlags.SetParticles(true)",
            "EngineShowFlags.SetNiagara(true)",
            "CreateDarkBackdrop",
            "/Engine/BasicShapes/Plane.Plane",
            "ObserveParticles",
            "WaitForConcurrentTickAndFinalize",
            "GetTotalSpawnedParticles",
            "TotalSpawnedParticles",
            "RuntimeEmitterInstances",
            "bAutoActivate = false",
            "DeactivateImmediate",
            "SetForceSolo(true)",
            "SetAutoDestroy(false)",
            "World->Tick(LEVELTICK_All",
            "AdvanceSimulation(1, 1.0f / 60.0f)",
            "system_emits_no_particles",
            "viewport_unavailable",
            "visible_fire_signature_not_observed",
            "programmatic_pixel_validation",
            "UEREMCP_POC_B10_EVIDENCE",
            "UEREMCP_POC_B10_OUTCOME=PASS",
            "UEREMCP_POC_B10_OUTCOME=FAIL",
            "Actor->Destroy()",
            "RemoveRealtimeOverride",
        ):
            self.assertIn(token, source)
        self.assertNotIn("ProcessThreadUntilIdle(", source)

    def test_runner_enables_rendering_and_requires_pass_artifact(self):
        runner = (
            REPO_ROOT / "tests" / "run_poc_b10_visible_render.ps1"
        ).read_text(encoding="utf-8")
        for token in (
            "UnrealEditor.exe",
            "-WithRendering",
            "UEREMCP.Niagara.POCB.VisibleRender",
            "UeremcpPocB10Output",
            "UeremcpPocB10System",
            "[string]$SystemPath",
            "UEREMCP_POC_B10_OUTCOME",
            "UEREMCP_POC_B10_RUNNER",
            "Test-Path $artifact",
        ):
            self.assertIn(token, runner)

    def test_shared_editor_runner_only_uses_nullrhi_without_rendering(self):
        runner = (REPO_ROOT / "tests" / "run_editor_tests.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn("[switch]$WithRendering", runner)
        self.assertIn("if (-not $WithRendering)", runner)
        self.assertIn("UEREMCP_POC_B10_(EVIDENCE|OUTCOME)", runner)


if __name__ == "__main__":
    unittest.main()
