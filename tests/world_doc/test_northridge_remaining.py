"""Acceptance-oriented checks for RB-Northridge-remaining-impl.md."""
from __future__ import annotations

import json
import os
import sys
import unittest

# tests/world_doc/ -> repo root is two levels up
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools"))

from check_operation_catalog import registered_plan_actions  # noqa: E402

CATALOG = os.path.join(REPO, "tools", "intent_router", "operation_catalog.json")
SCHEMA = os.path.join(REPO, "schemas", "envelope", "response.schema.json")
SCHEMA_PUBLISH = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpCore", "Private",
    "UeremcpSchemaPublishing.cpp")
WORLD_OPS = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpWorldOps.cpp")
PLACE = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpPlacePrefab.cpp")
ASSET_PROBE = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpAssetProbe.cpp")
IMPORT_MESH = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpImportMesh.cpp")
PAINT = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpLandscapePaint.cpp")
INTENT_ROUTER = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpCore", "Private",
    "UeremcpIntentRouter.cpp")
ENV_SERVICE = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpEnvironmentService.cpp")


def catalog():
    with open(CATALOG, encoding="utf-8") as fh:
        return json.load(fh)


def read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


class TestAssetRolePatterns(unittest.TestCase):
    def test_role_table_in_catalog(self):
        roles = catalog()["asset_role_patterns"]
        self.assertIn("*Pine*", roles["foliage.tree"])
        self.assertIn("*Grass*", roles["foliage.grass"])
        self.assertIn("*Wall*", roles["structure.wall"])

    def test_find_project_assets_operation(self):
        op = next(o for o in catalog()["operations"] if o["action"] == "find_project_assets")
        self.assertFalse(op["destructive"])
        self.assertIn("unresolved", op["recovery"])

    def test_probe_checks_is_loading_assets(self):
        body = read(ASSET_PROBE)
        self.assertIn("IsLoadingAssets", body)
        self.assertIn("partially_completed", body)
        self.assertIn("ASSET_REGISTRY_LOADING", body)


class TestErrorSchema(unittest.TestCase):
    def test_error_code_enum(self):
        with open(SCHEMA, encoding="utf-8") as fh:
            schema = json.load(fh)
        codes = schema["properties"]["error"]["properties"]["code"]["enum"]
        for required in (
            "NONUNIFORM_SCALE", "NEEDLE_SCALE_Z", "HEIGHTMAP_MISMATCH",
            "REALISM_GATE", "MESH_PATH_MISSING", "MESH_BOUNDS_MISMATCH",
        ):
            self.assertIn(required, codes)
        self.assertIn("next_args", schema["properties"]["error"]["properties"])

    def test_scale_rejection_carries_next_args(self):
        body = read(ENV_SERVICE)
        self.assertIn('TEXT("NONUNIFORM_SCALE")', body)
        self.assertIn('TEXT("NEEDLE_SCALE_Z")', body)
        self.assertIn('TEXT("HEIGHTMAP_MISMATCH")', body)
        self.assertIn("NextArgs", body)


class TestDescribeSlimContract(unittest.TestCase):
    def test_content_schema_mirror_removed(self):
        body = read(SCHEMA_PUBLISH)
        self.assertNotIn('SetObjectField(TEXT("contentSchema")', body)
        self.assertIn("contentSchema mirror REMOVED", body)

    def test_describe_operation_documents_detail(self):
        op = next(o for o in catalog()["operations"] if o["action"] == "describe_operation")
        self.assertIn("slim", op["recovery"])
        self.assertEqual(op["example_request"]["specification"].get("detail"), "slim")

    def test_describe_has_if_none_match_and_hash(self):
        body = read(INTENT_ROUTER)
        self.assertIn("if_none_match", body)
        self.assertIn("content_hash", body)
        self.assertIn('TEXT("index")', body)
        self.assertIn('TEXT("slim")', body)
        self.assertIn('TEXT("full")', body)


class TestPlacePrefabReusesLandscapeZ(unittest.TestCase):
    def test_place_calls_shared_helper(self):
        body = read(PLACE)
        self.assertIn("UeremcpWorldOps::LandscapeZAt", body)
        self.assertIn("flatten_pad", body)
        self.assertIn("FLATTEN_PAD_UNSUPPORTED", body)

    def test_world_ops_exports_helper(self):
        body = read(WORLD_OPS)
        self.assertIn("bool LandscapeZAt(", body)
        self.assertIn("ClearFoliageInBoxes", body)


class TestImportAndPaint(unittest.TestCase):
    def test_import_composes_static_mesh_tools(self):
        body = read(IMPORT_MESH)
        self.assertIn("StaticMeshTools", body)
        self.assertIn("import_file", body)
        self.assertIn("MESH_BOUNDS_MISMATCH", body)
        self.assertIn("expected_bounds_m", body)
        self.assertIn("CTF_UseComplexAsSimple", body)

    def test_paint_reads_live_landscape(self):
        body = read(PAINT)
        self.assertIn("GetHeightData", body)
        self.assertIn("SetAlphaData", body)
        self.assertIn("fallback", body)
        self.assertNotIn("GenerateHeightmap", body)


class TestNewActionsPlanRegistered(unittest.TestCase):
    def test_mutating_new_actions_have_plan_handlers(self):
        plan = registered_plan_actions()
        for action in (
            "find_project_assets", "import_mesh_for_world",
            "place_prefab_on_landscape", "paint_landscape_layers",
        ):
            self.assertIn(action, plan, action)

    def test_catalog_lists_all_six_impl_actions(self):
        actions = {o["action"] for o in catalog()["operations"]}
        for action in (
            "find_project_assets", "import_mesh_for_world",
            "place_prefab_on_landscape", "paint_landscape_layers",
            "describe_operation",
        ):
            self.assertIn(action, actions)


class TestExternalCapabilitiesHonesty(unittest.TestCase):
    def test_get_started_marks_watch_and_semantic_unavailable(self):
        body = read(INTENT_ROUTER)
        self.assertIn("external_mcp_capabilities", body)
        self.assertIn("user-unreal-watch", body)
        self.assertIn("SemanticSearch", body)
        self.assertIn('SetBoolField(TEXT("available"), false)', body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
