"""Acceptance checks for UEREMCP UI domain (UI-MCP-001…017 field-report fix)."""
from __future__ import annotations

import json
import os
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools"))

from check_operation_catalog import registered_plan_actions  # noqa: E402

CATALOG = os.path.join(REPO, "tools", "intent_router", "operation_catalog.json")
SCHEMA = os.path.join(REPO, "schemas", "envelope", "response.schema.json")
INTENT_ROUTER = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpCore", "Private",
    "UeremcpIntentRouter.cpp")
UI_TOOLSET_H = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpUI", "Public",
    "UeremcpUIToolset.h")
UI_OPS = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpUI", "Private",
    "UeremcpUIOps.cpp")
UI_INV = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpUI", "Private",
    "UeremcpUIInventoryOps.cpp")
UI_PLAN = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpUI", "Private",
    "UeremcpUIPlanHandlers.cpp")
UPLUGIN = os.path.join(REPO, "Plugins", "UEREMCP", "UEREMCP.uplugin")

UI_ACTIONS = [
    "create_widget_from_spec",
    "apply_ui_theme",
    "show_widget_in_world",
    "show_widget_on_screen",
    "capture_ui_frame",
    "spawn_character_preview",
    "save_widget_asset",
    "create_inventory_sheet_from_spec",
    "set_slot_icon",
    "set_container_weight",
    "set_slot_layout",
]


def catalog():
    with open(CATALOG, encoding="utf-8") as fh:
        return json.load(fh)


def read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


class TestUIDomainRegistration(unittest.TestCase):
    def test_uplugin_lists_module(self):
        body = read(UPLUGIN)
        self.assertIn('"Name": "UeremcpUI"', body)

    def test_toolset_declares_goal_ops(self):
        body = read(UI_TOOLSET_H)
        for name in (
            "CreateWidgetFromSpec",
            "ShowWidgetInWorld",
            "ShowWidgetOnScreen",
            "CaptureUiFrame",
            "SpawnCharacterPreview",
            "SaveWidgetAsset",
            "CreateInventorySheetFromSpec",
        ):
            self.assertIn(name, body)

    def test_get_started_lists_ui_domain(self):
        body = read(INTENT_ROUTER)
        self.assertIn("UeremcpUI.UeremcpUIToolset", body)
        self.assertIn("ui_domain", body)
        self.assertIn("show_widget_in_world", body)


class TestUICatalogRouting(unittest.TestCase):
    def test_all_ui_actions_catalogued(self):
        actions = {o["action"] for o in catalog()["operations"] if o.get("action")}
        for action in UI_ACTIONS:
            self.assertIn(action, actions, msg=action)

    def test_create_widget_use_when_covers_start_menu(self):
        op = next(o for o in catalog()["operations"] if o["action"] == "create_widget_from_spec")
        joined = " ".join(op["use_when"]).lower()
        self.assertIn("start menu", joined)
        self.assertTrue(op["qualified"].startswith("UeremcpUI."))

    def test_inventory_use_when(self):
        op = next(
            o for o in catalog()["operations"]
            if o["action"] == "create_inventory_sheet_from_spec")
        joined = " ".join(op["use_when"]).lower()
        self.assertIn("inventory", joined)
        self.assertIn("paper doll", joined)

    def test_plan_handlers_registered(self):
        registered = registered_plan_actions()
        for action in UI_ACTIONS:
            self.assertIn(action, registered, msg=action)

    def test_plan_handler_source_binds(self):
        body = read(UI_PLAN)
        for action in UI_ACTIONS:
            self.assertIn('TEXT("%s")' % action, body)


class TestUIHonestyContracts(unittest.TestCase):
    def test_screen_umg_error_code_in_schema(self):
        with open(SCHEMA, encoding="utf-8") as fh:
            schema = json.load(fh)
        codes = schema["properties"]["error"]["properties"]["code"]["enum"]
        for required in (
            "SCREEN_UMG_CAPTURE_UNSUPPORTED",
            "COMMON_UI_UNAVAILABLE",
            "PREVIEW_NOT_VISIBLE",
            "ICON_NOT_FOUND",
        ):
            self.assertIn(required, codes)

    def test_capture_rejects_screen_umg(self):
        body = read(UI_OPS)
        self.assertIn("SCREEN_UMG_CAPTURE_UNSUPPORTED", body)
        self.assertIn("screen_space_umg", body)

    def test_create_defaults_save_true(self):
        body = read(UI_OPS)
        self.assertIn('OptionsFlag(Request, TEXT("save"), true)', body)

    def test_inventory_slot_naming(self):
        body = read(UI_INV)
        self.assertIn("Slot_%s_r%d_c%d", body)
        self.assertIn("Slot_Hotbar_", body)
        self.assertIn("Slot_Equip_", body)

    def test_preview_reports_mesh_source(self):
        body = read(UI_OPS)
        self.assertIn("mesh_source", body)
        self.assertIn("proxy", body)


if __name__ == "__main__":
    unittest.main()
