#!/usr/bin/env python3
"""Out-of-editor pure-logic tests for UEREMCP validation harness (RB-14 q10).

These run with plain Python — no Unreal Editor. They cover harness conventions,
scratch-path policy, and change-info → changeEntry mapping shape that WS-05 needs.

Run:
    python tests/unit/test_harness_conventions.py
    python -m unittest discover -s tests/unit -v
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# Mirrors Plugins/.../UeremcpScratchPaths.h — keep in sync.
TESTS_CONTENT_ROOT = "/Game/__UeremcpTests"
POC_CONTENT_ROOT = "/Game/__UeremcpPoc"


def make_scratch_package_path(suite: str, name: str) -> str:
    if not name:
        raise ValueError("name required")
    if not suite:
        return f"{TESTS_CONTENT_ROOT}/{name}"
    return f"{TESTS_CONTENT_ROOT}/{suite}/{name}"


def is_safe_scratch_path(soft_path: str) -> bool:
    """True iff the soft path is under the tests root and not the POC root."""
    if soft_path.startswith(POC_CONTENT_ROOT):
        return False
    return soft_path.startswith(TESTS_CONTENT_ROOT + "/") or soft_path == TESTS_CONTENT_ROOT


# FSandboxedFileChangeInfo → envelope changeEntry mapping draft for WS-05.
# Fields verified in SandboxedFileChangeInfo.h: Path, Action, Timestamp (optional).
# [VERIFIED: $FS/.../Public/Types/SandboxedFileChangeInfo.h]
SANDBOX_ACTION_TO_CHANGE_KIND = {
    "None": None,
    "Added": "created",
    "Removed": "deleted",
    "Edited": "modified",
}


def map_sandboxed_change_to_entry(info: dict) -> dict | None:
    """Map one FSandboxedFileChangeInfo-like dict to a changeEntry-shaped dict.

    Missing fields that ADR-0005 / envelope may want (asset class, previous
    revision) are NOT invented — they are listed under `missing` so WS-05 can
    supplement from domain services.
    """
    action = info.get("action", "None")
    kind = SANDBOX_ACTION_TO_CHANGE_KIND.get(action)
    if kind is None:
        return None
    entry = {
        "kind": kind,
        "path": info.get("path", ""),
        "source": "FileSandbox.GetChanges",
    }
    if "timestamp" in info:
        entry["timestamp"] = info["timestamp"]
    entry["missing"] = [
        "asset_class",
        "previous_revision",
        "package_name",  # Path is absolute filesystem; soft path needs conversion
    ]
    return entry


class ScratchPathConventionsTest(unittest.TestCase):
    def test_make_path_with_suite(self):
        self.assertEqual(
            make_scratch_package_path("Rollback_MultiAssetDiscard", "DiscardAsset_0"),
            "/Game/__UeremcpTests/Rollback_MultiAssetDiscard/DiscardAsset_0",
        )

    def test_rejects_empty_name(self):
        with self.assertRaises(ValueError):
            make_scratch_package_path("Suite", "")

    def test_safe_paths(self):
        self.assertTrue(is_safe_scratch_path("/Game/__UeremcpTests/foo"))
        self.assertFalse(is_safe_scratch_path("/Game/Characters/Hero"))
        self.assertFalse(is_safe_scratch_path("/Game/__UeremcpPoc/Fireball"))

    def test_readme_documents_roots(self):
        readme = (REPO_ROOT / "tests" / "README.md").read_text(encoding="utf-8")
        self.assertIn("/Game/__UeremcpTests/", readme)


class SandboxChangeMappingTest(unittest.TestCase):
    def test_added_maps_to_created(self):
        entry = map_sandboxed_change_to_entry(
            {
                "path": r"C:\Proj\Content\__UeremcpTests\A.uasset",
                "action": "Added",
                "timestamp": "2026-07-29T00:00:00Z",
            }
        )
        assert entry is not None
        self.assertEqual(entry["kind"], "created")
        self.assertIn("asset_class", entry["missing"])
        self.assertIn("previous_revision", entry["missing"])

    def test_none_action_skipped(self):
        self.assertIsNone(map_sandboxed_change_to_entry({"path": "x", "action": "None"}))

    def test_mapping_json_roundtrip_stable(self):
        entry = map_sandboxed_change_to_entry({"path": "/tmp/a.uasset", "action": "Edited"})
        blob = json.dumps(entry, sort_keys=True)
        self.assertIn('"kind": "modified"', blob)


class HarnessLayoutTest(unittest.TestCase):
    def test_integration_rollback_doc_exists(self):
        path = REPO_ROOT / "tests" / "integration" / "Rollback.MultiAssetDiscard.md"
        self.assertTrue(path.is_file(), f"missing {path}")

    def test_run_scripts_exist(self):
        self.assertTrue((REPO_ROOT / "tests" / "run_unit_tests.py").is_file())
        self.assertTrue((REPO_ROOT / "tests" / "run_editor_tests.ps1").is_file())


if __name__ == "__main__":
    unittest.main()
