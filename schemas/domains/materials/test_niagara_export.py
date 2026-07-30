#!/usr/bin/env python3
"""Offline tests for WS-08 Niagara material export contract (WS-07 binding).

Mirrors path/role helpers documented in ws-08-niagara-material-export-handoff.md.

Usage::

    python schemas/domains/materials/test_niagara_export.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
HANDOFF = REPO_ROOT / "docs/proposals/ws-08-niagara-material-export-handoff.md"
EXPORT_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialNiagaraExport.cpp"
SERVICE_H = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialService.h"

MATERIALS_ROOT = "/Game/__UeremcpTests/Materials"

ROLE_PURPOSE = {
    "core": "elemental_projectile_core",
    "core_material": "elemental_projectile_core",
    "ribbon_trail": "elemental_projectile_trail",
    "trail": "elemental_projectile_trail",
    "trail_material": "elemental_projectile_trail",
}


def sanitize_token(token: str) -> str:
    out = "".join(c if c.isalnum() or c == "_" else "_" for c in token)
    return out or "Unnamed"


def resolve_mi_path(niagara_name: str, role: str) -> str:
    safe_name = sanitize_token(niagara_name)
    safe_role = sanitize_token(role)
    return f"{MATERIALS_ROOT}/MI_{safe_name}_{safe_role}"


class NiagaraExportContractTests(unittest.TestCase):
    def test_handoff_exists(self) -> None:
        self.assertTrue(HANDOFF.is_file(), "WS-08 Niagara export handoff doc must exist")

    def test_service_api_exported(self) -> None:
        header = SERVICE_H.read_text(encoding="utf-8")
        self.assertIn("UEREMCPMATERIAL_API FUeremcpMaterialCreateResult", header)
        self.assertIn("UEREMCPMATERIAL_API FUeremcpMaterialCreateResult ExecuteCreateVfxMaterial", header)

    def test_niagara_export_symbols_in_cpp(self) -> None:
        cpp = EXPORT_CPP.read_text(encoding="utf-8")
        for symbol in (
            "ResolveMaterialInstancePath",
            "ResolvePurposeForNiagaraRole",
            "VerifyPrimaryAssetIsMaterialInterface",
            "ExecuteCreateVfxMaterialForNiagaraRole",
        ):
            self.assertIn(symbol, cpp, f"missing export helper {symbol}")

    def test_deterministic_mi_paths(self) -> None:
        self.assertEqual(
            resolve_mi_path("NS_POCB_Fireball", "ribbon_trail"),
            f"{MATERIALS_ROOT}/MI_NS_POCB_Fireball_ribbon_trail",
        )
        self.assertEqual(
            resolve_mi_path("NS POCB!", "core"),
            f"{MATERIALS_ROOT}/MI_NS_POCB__core",
        )

    def test_role_purpose_table_documented(self) -> None:
        readme = HANDOFF.read_text(encoding="utf-8")
        for role, purpose in ROLE_PURPOSE.items():
            self.assertIn(role, readme)
            self.assertIn(purpose, readme)

    def test_ws07_example_path_pattern(self) -> None:
        # Matches ws-07-niagara-material-bindings.md inline creation target pattern.
        path = resolve_mi_path("NS_POCB_Fireball", "core")
        self.assertTrue(re.fullmatch(r"/Game/__UeremcpTests/Materials/MI_[A-Za-z0-9_]+", path))


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
