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
            "programmatic_pixel_validation",
            "UEREMCP_POC_B10_EVIDENCE",
            "UEREMCP_POC_B10_OUTCOME=PASS",
            "UEREMCP_POC_B10_OUTCOME=FAIL",
            "Actor->Destroy()",
            "RemoveRealtimeOverride",
        ):
            self.assertIn(token, source)

    def test_runner_enables_rendering_and_requires_pass_artifact(self):
        runner = (
            REPO_ROOT / "tests" / "run_poc_b10_visible_render.ps1"
        ).read_text(encoding="utf-8")
        for token in (
            "UnrealEditor.exe",
            "-WithRendering",
            "UEREMCP.Niagara.POCB.VisibleRender",
            "UeremcpPocB10Output",
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
