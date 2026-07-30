"""Fast source/schema checks for WS-10 animation scaffolding."""

from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Registry, Resource


MODULE = Path(__file__).resolve().parents[1]
REPO = MODULE.parents[3]
PROTOCOL_TESTS = REPO / "Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py"
sys.path.insert(0, str(PROTOCOL_TESTS))

from ueremcp_protocol.content_hash import content_hash  # noqa: E402


def asset_state_validator() -> tuple[Draft202012Validator, dict]:
    schema_path = (
        REPO
        / "schemas/domains/animation/inspect_montage.asset-state.schema.json"
    )
    common_path = REPO / "schemas/common/defs.schema.json"
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    common = json.loads(common_path.read_text(encoding="utf-8"))
    registry = Registry().with_resource(
        common["$id"],
        Resource.from_contents(common),
    )
    return Draft202012Validator(schema, registry=registry), schema


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
                "track_index": 0,
                "trigger_chance": 1.0,
                "trigger_on_dedicated_server": True,
                "is_state": False,
                "class": "/Script/Engine.AnimNotify_ResetDynamics",
            }
        ],
    }


def anim_bp_inventory() -> dict:
    return {
        "asset_path": "/Game/__UeremcpTests/Animation/ABP_WS10_Inspect",
        "asset_class": "/Script/Engine.AnimBlueprint",
        "is_template": True,
        "skeleton": "",
        "graphs": [
            {
                "name": "AnimGraph",
                "graph_class": "/Script/AnimGraph.AnimationGraph",
                "graph_type": "AnimBlueprintGraph",
                "node_count": 1,
                "is_animation_graph": True,
                "fidelity": {
                    "inventory_complete": True,
                    "nodes_emitted": False,
                    "links_emitted": False,
                    "round_trip_supported": False,
                },
            },
            {
                "name": "Locomotion",
                "graph_class": "/Script/AnimGraph.AnimationStateMachineGraph",
                "graph_type": "AnimStateMachine",
                "node_count": 0,
                "is_animation_graph": False,
                "fidelity": {
                    "inventory_complete": True,
                    "nodes_emitted": False,
                    "links_emitted": False,
                    "round_trip_supported": False,
                },
            },
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
        for diagnostic in (
            "Inspection.SlotCount",
            "Inspection.SegmentCount",
            "Inspection.SectionCount",
            "Inspection.NotifyCount",
            "Response.Dependencies",
            "Response.Revision",
            "Response.CapabilityNotes",
            "animation.montage.loaded",
            "animation.montage.slots_enumerated",
            "animation.montage.real_notifies_enumerated",
            "animation.montage.content_hash_computed",
        ):
            self.assertIn(diagnostic, source)

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
            "earlier trigger is serialized first",
            "invalid track index is retained",
            "notify without object has null class",
            "raw notify storage order does not change revision",
        ):
            self.assertIn(expected, source)
        service = (
            MODULE / "Private/UeremcpAnimationService.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("NotifyEvents.Sort()", service)

    def test_asset_state_schema_accepts_emitted_shape(self) -> None:
        validator, schema = asset_state_validator()
        self.assertEqual(list(validator.iter_errors(schema["examples"][0])), [])

    def test_asset_state_schema_rejects_missing_and_unknown_fields(self) -> None:
        validator, schema = asset_state_validator()
        malformed = copy.deepcopy(schema["examples"][0])
        malformed.pop("notifies")
        malformed["raw_unreal_dump"] = {}
        malformed["revision"] = None
        errors = list(validator.iter_errors(malformed))
        self.assertGreaterEqual(len(errors), 3)

    def test_asset_state_schema_matches_service_emission_contract(self) -> None:
        _, schema = asset_state_validator()
        expected_top_level = {
            "asset_path",
            "asset_class",
            "length_seconds",
            "auto_blend_out",
            "sync_group",
            "skeleton",
            "slots",
            "sections",
            "notifies",
            "content_hash",
            "revision",
        }
        self.assertEqual(set(schema["required"]), expected_top_level)
        self.assertEqual(set(schema["properties"]), expected_top_level)
        self.assertEqual(
            schema["properties"]["revision"]["$ref"],
            "../../common/defs.schema.json#/$defs/contentHash",
        )

        expected_nested_fields = {
            "slot": {"name", "segments"},
            "segment": {
                "animation",
                "start_time",
                "animation_start_time",
                "animation_end_time",
                "play_rate",
                "loop_count",
            },
            "section": {"name", "start_time", "next_section"},
            "notify": {
                "name",
                "time",
                "duration",
                "track",
                "track_index",
                "trigger_chance",
                "trigger_on_dedicated_server",
                "is_state",
                "class",
            },
        }
        expected_nested_required = copy.deepcopy(expected_nested_fields)
        expected_nested_required["section"].remove("next_section")
        for definition, fields in expected_nested_fields.items():
            with self.subTest(definition=definition):
                self.assertEqual(
                    set(schema["$defs"][definition]["properties"]),
                    fields,
                )
                self.assertEqual(
                    set(schema["$defs"][definition]["required"]),
                    expected_nested_required[definition],
                )

        source = (
            MODULE / "Private/UeremcpAnimationService.cpp"
        ).read_text(encoding="utf-8")
        for field in expected_top_level | set().union(*expected_nested_fields.values()):
            with self.subTest(emitted_field=field):
                self.assertIn(f'TEXT("{field}")', source)

    def test_asset_state_schema_closes_every_object_shape(self) -> None:
        _, schema = asset_state_validator()

        def assert_closed(value: object, path: str) -> None:
            if isinstance(value, dict):
                if value.get("type") == "object":
                    self.assertIs(
                        value.get("additionalProperties"),
                        False,
                        f"open object schema at {path}",
                    )
                for key, child in value.items():
                    assert_closed(child, f"{path}.{key}")
            elif isinstance(value, list):
                for index, child in enumerate(value):
                    assert_closed(child, f"{path}[{index}]")

        assert_closed(schema, "$")

    def test_schema_identifies_adr_0011_as_proposed(self) -> None:
        _, schema = asset_state_validator()
        self.assertIn("Proposed ADR-0011", schema["description"])
        self.assertIn("Proposed ADR-0011 only", schema["$comment"])
        self.assertIn("Do not emit", schema["$comment"])

    def test_editor_fixture_is_scratch_only_and_unsaved(self) -> None:
        source = (
            MODULE / "Private/Tests/UeremcpAnimationTests.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("/Game/__UeremcpTests/Animation/", source)
        self.assertIn("EditorScratchAsset", source)
        self.assertIn("response remains honest before asset_state amendment", source)
        self.assertIn("package and object paths produce one canonical revision", source)
        tool_source = (
            MODULE / "Private/UeremcpAnimationToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("FPackageName::GetLongPackageAssetName", tool_source)
        self.assertIn("FPackageName::ObjectPathToPackageName", tool_source)
        self.assertNotIn("SavePackage(", source)
        self.assertNotIn("SaveAsset(", source)

    def test_read_anim_bp_spec_is_complete_state_request(self) -> None:
        schema = json.loads(
            (REPO / "schemas/domains/animation/read_anim_bp.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(schema["type"], "object")
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(schema["properties"], {})
        self.assertIn("read-only", schema["description"])
        self.assertIn("shared UEdGraph reader", schema["description"])
        self.assertIn("does not use blueprint dsl", schema["description"].lower())

    def test_read_anim_bp_service_uses_shared_graph_reader(self) -> None:
        source = (
            MODULE / "Private/UeremcpAnimationService.cpp"
        ).read_text(encoding="utf-8")
        for expected in (
            "GetAllGraphs",
            "UAnimationStateMachineGraph",
            "FUeremcpEdGraphReader::ReadGraph",
            "FUeremcpEdGraphSemanticHooks",
            'TEXT("animation")',
            "Options.GraphType = GraphType",
            "Options.bRoundTripSupported = false",
            "InspectAnimBlueprint",
        ):
            self.assertIn(expected, source)
        self.assertNotIn('TEXT("graph_guid")', source)
        self.assertNotIn("Graph->GraphGuid", source)
        self.assertNotIn("read_graph_dsl", source.lower())

    def test_read_anim_bp_tool_stays_partial_and_withholds_asset_state(self) -> None:
        source = (
            MODULE / "Private/UeremcpAnimationToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("ReadAnimBp", source)
        self.assertIn('TEXT("read_anim_bp")', source)
        self.assertIn('Response.Status = TEXT("partially_completed")', source)
        self.assertIn("animation.anim_bp.nodes_and_links_read", source)
        self.assertIn("animation.anim_bp.extensions_animation_added", source)
        self.assertNotIn('SetObjectField(TEXT("asset_state")', source)
        build = MODULE.joinpath("UeremcpAnimation.Build.cs").read_text(encoding="utf-8")
        self.assertIn("AnimGraph", build)
        self.assertIn("UeremcpBlueprint", build)

    def test_read_anim_bp_cpp_coverage_and_editor_scratch(self) -> None:
        source = (
            MODULE / "Private/Tests/UeremcpAnimationTests.cpp"
        ).read_text(encoding="utf-8")
        for expected in (
            "UEREMCP.Animation.ReadAnimBp.GraphInventory",
            "UEREMCP.Animation.ReadAnimBp.EditorScratchAsset",
            "UEREMCP.Animation.ReadAnimBp.EnvelopeRejections",
            "stable AnimBP revision",
            "engine graph GUID churn does not change semantic revision",
            "node-count semantic change updates revision",
            "nested state machine classified",
            "every graph carries round-trip fidelity",
            "every graph emits nodes and links",
            "every graph carries extensions.animation",
            "response remains honest before asset_state amendment",
            "animation.anim_bp.nodes_and_links_read",
            "UEREMCP.Animation.CrossAsset.MontageAndAnimBpIsolation",
            "cross-fixtures report the same skeleton dependency",
            "different asset shapes retain independent revisions",
            "null rejection clears stale dependencies",
        ):
            self.assertIn(expected, source)
        self.assertNotIn("SavePackage(", source)

    def test_anim_bp_revision_tracks_inventory_semantics(self) -> None:
        first = anim_bp_inventory()
        node_change = copy.deepcopy(first)
        node_change["graphs"][0]["node_count"] = 2
        self.assertNotEqual(content_hash(first), content_hash(node_change))

        classification_change = copy.deepcopy(first)
        classification_change["graphs"][1]["graph_type"] = "EdGraph"
        self.assertNotEqual(content_hash(first), content_hash(classification_change))


if __name__ == "__main__":
    unittest.main()
