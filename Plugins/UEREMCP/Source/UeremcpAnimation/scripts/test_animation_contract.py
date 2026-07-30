"""Fast source/schema checks for WS-10 animation scaffolding."""

from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path


MODULE = Path(__file__).resolve().parents[1]
REPO = MODULE.parents[3]
PROTOCOL_TESTS = REPO / "Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py"
sys.path.insert(0, str(PROTOCOL_TESTS))

from ueremcp_protocol.content_hash import content_hash  # noqa: E402


def montage_state() -> dict:
    return {
        "asset_path": "/Game/__UeremcpTests/Animation/AM_WS10_Revision",
        "asset_class": "/Script/Engine.AnimMontage",
        "length_seconds": 1.0,
        "auto_blend_out": True,
        "sync_group": "",
        "skeleton": "/Game/Characters/SK_Test",
        "slots": [
            {
                "name": "UpperBody",
                "segments": [
                    {
                        "animation": "/Game/Characters/A_Attack",
                        "start_time": 0.0,
                        "animation_start_time": 0.0,
                        "animation_end_time": 1.0,
                        "play_rate": 1.0,
                        "loop_count": 1,
                    }
                ],
            }
        ],
        "sections": [{"name": "Attack", "start_time": 0.0}],
        "notifies": [
            {
                "name": "Impact",
                "time": 0.5,
                "duration": 0.0,
                "track": "Gameplay",
                "trigger_chance": 1.0,
                "trigger_on_dedicated_server": True,
                "is_state": False,
                "class": "/Script/Engine.AnimNotify_ResetDynamics",
            }
        ],
    }


class AnimationContractTests(unittest.TestCase):
    def test_inspect_montage_spec_is_complete_state_request(self) -> None:
        schema = json.loads(
            (REPO / "schemas/domains/animation/inspect_montage.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(schema["type"], "object")
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(schema["properties"], {})

    def test_service_serializes_adjacent_montage_state(self) -> None:
        source = (
            MODULE / "Private/UeremcpAnimationService.cpp"
        ).read_text(encoding="utf-8")
        for field in (
            '"skeleton"',
            '"slots"',
            '"segments"',
            '"sections"',
            '"notifies"',
            '"content_hash"',
            '"revision"',
        ):
            self.assertIn(field, source)
        self.assertIn("GetAnimationNotifyEvents", source)

    def test_tool_reports_response_schema_blocker_honestly(self) -> None:
        source = (
            MODULE / "Private/UeremcpAnimationToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('Response.Status = TEXT("partially_completed")', source)
        self.assertIn("frozen response envelope", source)
        self.assertNotIn('SetObjectField(TEXT("asset_state")', source)

    def test_montage_revision_stable_across_envelope_and_guid_churn(self) -> None:
        first = montage_state()
        reread = copy.deepcopy(first)
        reread["content_hash"] = "sha256:stale"
        reread["revision"] = "sha256:stale"
        reread["notifies"][0]["guid"] = "regenerated-editor-guid"
        self.assertEqual(content_hash(first), content_hash(reread))

    def test_montage_revision_changes_with_notify_semantics(self) -> None:
        first = montage_state()
        changed = copy.deepcopy(first)
        changed["notifies"][0]["trigger_chance"] = 0.5
        self.assertNotEqual(content_hash(first), content_hash(changed))

    def test_cpp_contract_covers_notify_classes_and_revision_stability(self) -> None:
        source = (
            MODULE / "Private/Tests/UeremcpAnimationTests.cpp"
        ).read_text(encoding="utf-8")
        for expected in (
            "AnimNotify_ResetDynamics",
            "AnimNotifyState_DisableRootMotion",
            "notify concrete class",
            "state concrete class",
            "unchanged montage has stable revision",
            "editor notify GUID does not change revision",
            "notify trigger policy changes revision",
            "restoring semantic state restores revision",
        ):
            self.assertIn(expected, source)


if __name__ == "__main__":
    unittest.main()
