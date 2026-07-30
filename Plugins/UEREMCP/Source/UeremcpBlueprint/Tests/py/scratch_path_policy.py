"""Scratch-path policy mirrors for WS-06 Blueprint submit_graph (offline tests).

Keep in sync with:
- FUeremcpBlueprintGraphWriter::IsScratchAssetPath
- UeremcpScratchPaths.h (RB-14)
- tests/unit/test_harness_conventions.py (WS-11)
"""

from __future__ import annotations

TESTS_CONTENT_ROOT = "/Game/__UeremcpTests"
POC_CONTENT_ROOT = "/Game/__UeremcpPoc"
BLUEPRINT_SUBMIT_PREFIX = f"{TESTS_CONTENT_ROOT}/"


def is_blueprint_submit_scratch_path(asset_path: str) -> bool:
    """Mirror FUeremcpBlueprintGraphWriter::IsScratchAssetPath (prefix guard)."""
    return asset_path.startswith(BLUEPRINT_SUBMIT_PREFIX)


def is_ws11_safe_scratch_path(soft_path: str) -> bool:
    """Mirror tests/unit/test_harness_conventions.is_safe_scratch_path."""
    if soft_path.startswith(POC_CONTENT_ROOT):
        return False
    return soft_path.startswith(TESTS_CONTENT_ROOT + "/") or soft_path == TESTS_CONTENT_ROOT


def make_scratch_package_path(suite: str, name: str) -> str:
    """Mirror UeremcpMakeScratchPackagePath."""
    if not name:
        raise ValueError("name required")
    if not suite:
        return f"{TESTS_CONTENT_ROOT}/{name}"
    return f"{TESTS_CONTENT_ROOT}/{suite}/{name}"


def is_unsupported_control_flow_type_id(type_id: str) -> bool:
    """Mirror C++ translator rejection for branch/if/switch/multigate."""
    markers = (
        "FlowControl|Branch",
        "FlowControl|IfThenElse",
        "|Switch",
        "MultiGate",
        "FlowControl|Sequence",
    )
    return any(marker in type_id for marker in markers)
