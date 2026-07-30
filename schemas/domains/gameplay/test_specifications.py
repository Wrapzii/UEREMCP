#!/usr/bin/env python3
"""Unit tests for WS-09 gameplay specification schemas."""

from __future__ import annotations

import json
import re
import sys
import unittest
from pathlib import Path

try:
    import jsonschema
    from jsonschema import Draft202012Validator
    from referencing import Registry, Resource
except ImportError:
    sys.stderr.write("error: pip install jsonschema\n")
    raise SystemExit(1)

REPO_ROOT = Path(__file__).resolve().parents[3]
SCHEMAS_DIR = REPO_ROOT / "schemas"
GAMEPLAY_DIR = SCHEMAS_DIR / "domains" / "gameplay"
GAMEPLAY_SOURCE_DIR = (
    REPO_ROOT / "Plugins" / "UEREMCP" / "Source" / "UeremcpGameplay"
)
EXPECTED_FRE_ABILITY_FIELDS = frozenset(
    {
        "AbilityId",
        "DisplayName",
        "LineId",
        "Element",
        "Tier",
        "Wheel",
        "CastType",
        "CircleTier",
        "ElementColor",
        "CastTimeSec",
        "CooldownSec",
        "StaminaCost",
        "DurationSec",
        "EffectTag",
        "Speed",
        "Range",
        "ProjRadius",
        "GravityScale",
        "Homing",
        "ImpactDamage",
        "ImpactStatus",
        "StatusDuration",
        "AoeRadius",
        "EscalateTo",
        "SpawnEntity",
        "EntityLengthCm",
        "EntityThicknessCm",
        "EntityHeightCm",
        "CastNS",
        "ProjectileNS",
        "ImpactNS",
        "CircleMaterial",
        "VFXDefinition",
        "CircleDiameterCm",
        "AudioCueCast",
        "AudioCueTravel",
        "AudioCueImpact",
        "AudioCueFail",
        "UnlockSkillNode",
        "MinClassification",
    }
)


def load_registry() -> Registry:
    resources = []
    for path in sorted(SCHEMAS_DIR.rglob("*.schema.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        resources.append((schema["$id"], Resource.from_contents(schema)))
    return Registry().with_resources(resources)


class GameplaySpecificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.schema = json.loads(
            (GAMEPLAY_DIR / "create_spell.schema.json").read_text(encoding="utf-8")
        )
        cls.validator = Draft202012Validator(cls.schema, registry=load_registry())
        cls.example = cls.schema["examples"][0]

    def test_embedded_examples_validate(self) -> None:
        self.assertGreater(len(self.schema["examples"]), 0)
        for example in self.schema["examples"]:
            self.validator.validate(example)

    def test_pattern_b_is_not_advisory(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["networking"]["authority"] = "client"
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_row_name_is_stable_identifier(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["row_name"] = "Fireball 1"
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_status_uses_re_enum_name(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["impact"]["status"] = "Burning"
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_status_requires_positive_duration(self) -> None:
        for invalid_duration in (None, 0):
            bad = json.loads(json.dumps(self.example))
            if invalid_duration is None:
                bad["impact"].pop("status_duration")
            else:
                bad["impact"]["status_duration"] = invalid_duration
            with self.subTest(duration=invalid_duration):
                with self.assertRaises(jsonschema.ValidationError):
                    self.validator.validate(bad)

    def test_unknown_primitive_is_rejected(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["create_gameplay_effect"] = {}
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_optional_types_and_bounds_are_strict(self) -> None:
        mutations = (
            ("speed string", lambda spec: spec["delivery"].update(speed="fast")),
            ("negative cooldown", lambda spec: spec.setdefault("timing", {}).update(cooldown_sec=-1)),
            ("short wall", lambda spec: spec["delivery"].update(entity_height_cm=49)),
            ("zero circle", lambda spec: spec.setdefault("presentation", {}).update(circle_diameter_cm=0)),
        )
        for label, mutate in mutations:
            bad = json.loads(json.dumps(self.example))
            mutate(bad)
            with self.subTest(case=label):
                with self.assertRaises(jsonschema.ValidationError):
                    self.validator.validate(bad)

    def test_cpp_planner_maps_exact_verified_row_surface(self) -> None:
        planner = (
            GAMEPLAY_SOURCE_DIR / "Private" / "UeremcpSpellPlanner.cpp"
        ).read_text(encoding="utf-8")
        direct_fields = set(
            re.findall(r'Row->Set(?:String|Number|Array)Field\(TEXT\("([^"]+)"\)', planner)
        )
        dependency_fields = set(
            re.findall(
                r'AddDependencyIfPresent\(\s*Presentation,\s*TEXT\("[^"]+"\),'
                r'\s*TEXT\("([^"]+)"\)',
                planner,
            )
        )
        mapped_fields = frozenset(direct_fields | dependency_fields)
        self.assertEqual(EXPECTED_FRE_ABILITY_FIELDS, mapped_fields)

        re_header = (
            Path.home()
            / "Documents"
            / "Unreal Projects"
            / "RE"
            / "Source"
            / "RE"
            / "Public"
            / "REAbilityTypes.h"
        )
        if not re_header.is_file():
            self.skipTest("local REAbilityTypes.h unavailable for API drift check")
        header_text = re_header.read_text(encoding="utf-8")
        missing = sorted(
            field
            for field in EXPECTED_FRE_ABILITY_FIELDS
            if re.search(rf"\b{re.escape(field)}\b\s*(?:=|;)", header_text) is None
        )
        self.assertEqual([], missing, f"planner fields missing from FREAbilityDef: {missing}")

    def test_preflight_source_cannot_claim_mutation_success(self) -> None:
        toolset = (
            GAMEPLAY_SOURCE_DIR / "Private" / "UeremcpGameplayToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('Response.Status = TEXT("partially_completed")', toolset)
        self.assertNotIn("created_and_validated", toolset)
        self.assertNotIn("modified_and_validated", toolset)
        for evidence in (
            'SetNullField(TEXT("saved"))',
            'SetNullField(TEXT("reread_after_write"))',
            'SetArrayField(\n\t\tTEXT("changes")',
            'SetObjectField(TEXT("rollback")',
            'SetObjectField(TEXT("diagnostics")',
        ):
            self.assertIn(evidence, toolset)

    def test_gameplay_toolset_registers_on_post_engine_init(self) -> None:
        module = (
            GAMEPLAY_SOURCE_DIR / "Private" / "UeremcpGameplayModule.cpp"
        ).read_text(encoding="utf-8")
        for evidence in (
            "GetOnPostEngineInit().AddRaw",
            "RegisterToolsetClass(",
            "UUeremcpGameplayToolset::StaticClass()",
            "GetOnPostEngineInit().Remove(OnPostEngineInitHandle)",
            "UnregisterToolsetClass(",
        ):
            self.assertIn(evidence, module)

    def test_write_plan_captures_guarded_envelope_controls(self) -> None:
        header = (
            GAMEPLAY_SOURCE_DIR / "Public" / "UeremcpSpellPlanner.h"
        ).read_text(encoding="utf-8")
        planner = (
            GAMEPLAY_SOURCE_DIR / "Private" / "UeremcpSpellPlanner.cpp"
        ).read_text(encoding="utf-8")
        for field in (
            "RequestId",
            "Mode",
            "bDryRun",
            "bAtomic",
            "bSave",
            "bValidate",
            "bRollbackOnFailure",
            "TimeoutMs",
            "OnRevisionConflict",
            "ExpectedRevision",
            "bHasExpectedRevision",
            "IdempotencyKey",
        ):
            self.assertIn(field, header)
        for guarded_step in (
            "acquire_shared_mutator",
            "reject_foreign_active_sandbox",
            "enter_atomic_content_sandbox",
            "configure_rollback_on_failure",
            "check_expected_revision",
            "check_idempotency_replay",
            "save_table_to_sandbox",
            "reread_and_compare_normalized_row",
            "discard_dry_run",
            "release_shared_mutator",
        ):
            self.assertIn(guarded_step, planner)
        self.assertIn('TEXT("UeremcpCore.mutating_dispatcher")', planner)
        self.assertNotIn("UeremcpGameplay.module_registration", planner)

    def test_non_dry_write_is_queue_gated_and_audited(self) -> None:
        toolset = (
            GAMEPLAY_SOURCE_DIR / "Private" / "UeremcpGameplayToolset.cpp"
        ).read_text(encoding="utf-8")
        non_dry = toolset.index("if (!Request.bDryRun)")
        acquire = toolset.index("FUeremcpMutatorQueue::TryAcquire(", non_dry)
        audit = toolset.index("FUeremcpAuditLog::Append(", acquire)
        release = toolset.index(
            "FUeremcpMutatorQueue::Release(ProjectKey, Request.RequestId)",
            audit,
        )
        self.assertLess(non_dry, acquire)
        self.assertLess(acquire, audit)
        self.assertLess(audit, release)
        self.assertIn(
            "FUeremcpMutatorQueue::CancelQueued(ProjectKey, Request.RequestId)",
            toolset,
        )
        self.assertIn("Shared mutator queue is unavailable", toolset)
        self.assertIn("no write occurred", toolset)
        self.assertIn("UeremcpCore.mutating_dispatcher", (
            GAMEPLAY_SOURCE_DIR / "Private" / "UeremcpSpellPlanner.cpp"
        ).read_text(encoding="utf-8"))

    def test_dry_run_response_golden_is_complete_and_schema_valid(self) -> None:
        response_schema = json.loads(
            (SCHEMAS_DIR / "envelope" / "response.schema.json").read_text(
                encoding="utf-8"
            )
        )
        response = json.loads(
            (
                GAMEPLAY_DIR / "golden" / "dry_run_preflight.response.json"
            ).read_text(encoding="utf-8")
        )
        Draft202012Validator(
            response_schema, registry=load_registry()
        ).validate(response)
        self.assertEqual("partially_completed", response["status"])
        self.assertEqual([], response["changes"])
        self.assertIsNone(response["validation"]["saved"])
        self.assertIsNone(response["validation"]["reread_after_write"])
        self.assertFalse(response["rollback"]["available"])
        self.assertFalse(response["rollback"]["performed"])
        self.assertNotIn("created_assets", response["result"])
        self.assertNotIn("modified_assets", response["result"])
        write_note = response["understood"]["interpretation_notes"][1]
        for control in (
            "row_struct=/Script/RE.REAbilityDef",
            "mode=create_or_update",
            "dry_run=true",
            "atomic=true",
            "save=true",
            "validate=true",
            "rollback_on_failure=true",
            "timeout_ms=0",
            "on_revision_conflict=reject",
            "expected_revision=<absent>",
            "idempotency_key=<absent>",
        ):
            self.assertIn(control, write_note)
        self.assertEqual(2, len(response["result"]["dependencies"]))


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
