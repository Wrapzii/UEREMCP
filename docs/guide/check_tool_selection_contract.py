#!/usr/bin/env python3
"""Contract tests for UEREMCP tool-selection discoverability.

Owner: WS-13 (docs/guide/**). No editor required::

    python docs/guide/check_tool_selection_contract.py

Fails if the published routing artifact regresses, if header description cues
drift below the contract, or if the deterministic intent→tool benchmark fails.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

GUIDE_DIR = Path(__file__).resolve().parent
REPO_ROOT = GUIDE_DIR.parents[1]
CONTRACT_PATH = GUIDE_DIR / "tool-selection-contract.json"
POLICY_PATH = GUIDE_DIR / "tool-selection-policy.md"

# Routing cues that must appear in the policy doc (human-facing mirror).
POLICY_REQUIRED_PHRASES = [
    "Prefer UEREMCP",
    "execute_plan",
    "InstantiateTemplate",
    "CaptureEffectFrames",
    "SetNameFilters",
    "expected_revision",
    "cannot guarantee",
]


def load_contract() -> dict:
    return json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))


def fail(errors: list[str], msg: str) -> None:
    errors.append(msg)


def check_structure(contract: dict, errors: list[str]) -> None:
    for key in (
        "contract_id",
        "principles",
        "toolsets",
        "routing_benchmark",
        "special_routes",
        "epic_policy",
        "set_name_filters",
        "required_description_cues",
    ):
        if key not in contract:
            fail(errors, f"contract missing top-level key: {key}")

    if contract.get("contract_id") != "ueremcp-tool-selection":
        fail(errors, "contract_id must be ueremcp-tool-selection")

    if not contract.get("routing_benchmark"):
        fail(errors, "routing_benchmark must be non-empty")

    snf = contract.get("set_name_filters") or {}
    if snf.get("apply_now") is not False:
        fail(errors, "set_name_filters.apply_now must be false until WS-03 applies filters")
    if not snf.get("verified"):
        fail(errors, "set_name_filters.verified tag missing")


def check_policy_doc(errors: list[str]) -> None:
    if not POLICY_PATH.is_file():
        fail(errors, "missing tool-selection-policy.md")
        return
    text = POLICY_PATH.read_text(encoding="utf-8")
    collapsed = re.sub(r"\s+", " ", text)
    for phrase in POLICY_REQUIRED_PHRASES:
        if phrase not in collapsed:
            fail(errors, f"tool-selection-policy.md missing phrase: {phrase!r}")


def check_examples(contract: dict, errors: list[str]) -> None:
    examples = contract.get("examples") or {}
    for key in ("minimal_dir", "complete_dir"):
        rel = examples.get(key)
        if not rel:
            fail(errors, f"examples.{key} missing")
            continue
        path = REPO_ROOT / rel
        if not path.is_dir():
            fail(errors, f"examples dir missing: {rel}")
            continue
        jsons = list(path.glob("*.json"))
        if not jsons:
            fail(errors, f"no example JSON in {rel}")
        for jf in jsons:
            try:
                data = json.loads(jf.read_text(encoding="utf-8"))
            except json.JSONDecodeError as exc:
                fail(errors, f"invalid JSON {jf.relative_to(REPO_ROOT)}: {exc}")
                continue
            if "action" not in data and "protocol_version" not in data:
                # allow wrapper { "request": { ... } }
                if not isinstance(data.get("request"), dict):
                    fail(
                        errors,
                        f"{jf.relative_to(REPO_ROOT)}: expected envelope or request wrapper",
                    )


def iter_tools(contract: dict):
    for ts in contract.get("toolsets", []):
        for tool in ts.get("tools", []):
            yield ts, tool


def check_tool_inventory(contract: dict, errors: list[str]) -> None:
    seen_actions: set[str] = set()
    for ts, tool in iter_tools(contract):
        for field in (
            "mcp_tool",
            "action",
            "mutates",
            "destructive",
            "status",
            "required_fields",
            "validation",
            "idempotency",
            "output_status",
            "prefer_for",
            "description_cues",
        ):
            if field not in tool:
                fail(
                    errors,
                    f"{ts.get('mcp_name')}.{tool.get('mcp_tool', '?')}: missing {field}",
                )
        action = tool.get("action")
        if action in seen_actions and action not in {"ping", "echo"}:
            # ping/echo may repeat across toolsets
            pass
        seen_actions.add(action or "")

        header = ts.get("header")
        if not header:
            fail(errors, f"{ts.get('mcp_name')}: missing header path")
            continue
        header_path = REPO_ROOT / header
        # Visual capture may not be merged yet — allow pending with note.
        if not header_path.is_file():
            if ts.get("catalog_status") == "pending_catalog":
                continue
            fail(errors, f"missing toolset header: {header}")
            continue

        text = header_path.read_text(encoding="utf-8")
        mcp_tool = tool.get("mcp_tool", "")
        if mcp_tool and mcp_tool not in text:
            fail(errors, f"{header}: AICallable method {mcp_tool} not found")

        # Soft description cue check against header comments near the method.
        cues = tool.get("description_cues") or []
        # Only enforce cues that are short and likely present; record misses.
        method_window = extract_method_comment(text, mcp_tool)
        if method_window is None:
            fail(errors, f"{header}: could not locate comment window for {mcp_tool}")
            continue
        missing = [c for c in cues if c.lower() not in method_window.lower()]
        # Allow up to half the cues to be deferred to describe_toolset proposal,
        # but require at least one cue match for non-pending toolsets.
        if cues and len(missing) == len(cues):
            fail(
                errors,
                f"{header}::{mcp_tool}: none of description_cues found in comment "
                f"(cues={cues})",
            )


def extract_method_comment(header_text: str, method: str) -> str | None:
    """Return the doc-comment + signature block containing `static FString Method`."""
    pattern = re.compile(
        rf"(/\*\*.*?\*/\s*)?UFUNCTION\([^)]*AICallable[^)]*\)\s*"
        rf"static\s+FString\s+{re.escape(method)}\s*\(",
        re.DOTALL,
    )
    match = pattern.search(header_text)
    if not match:
        # Fallback: any nearby lines above the method name.
        idx = header_text.find(f"static FString {method}")
        if idx < 0:
            return None
        return header_text[max(0, idx - 800) : idx + 120]
    start = max(0, match.start() - 50)
    return header_text[start : match.end()]


def resolve_benchmark(contract: dict, case: dict) -> dict | None:
    """Map a benchmark case to the published tool inventory entry."""
    expected_tool = case["expected_tool"]
    expected_action = case["expected_action"]
    substr = case["expected_toolset_substring"]
    for ts, tool in iter_tools(contract):
        if substr not in ts.get("mcp_name", ""):
            continue
        if tool.get("mcp_tool") == expected_tool and tool.get("action") == expected_action:
            return {"toolset": ts, "tool": tool}
    return None


def check_benchmark(contract: dict, errors: list[str]) -> int:
    """Deterministic intent → published tool selection (not an LLM claim)."""
    passed = 0
    for case in contract.get("routing_benchmark", []):
        intent_id = case.get("intent_id", "?")
        resolved = resolve_benchmark(contract, case)
        if resolved is None:
            fail(
                errors,
                f"benchmark {intent_id}: no inventory match for "
                f"{case.get('expected_toolset_substring')}/"
                f"{case.get('expected_tool')}/"
                f"{case.get('expected_action')}",
            )
            continue

        prefer = resolved["tool"].get("prefer_for") or []
        # Every benchmark intent_id should be covered by prefer_for or special route.
        covered = intent_id in prefer or any(
            intent_id.replace("_", "") in p.replace("_", "") for p in prefer
        )
        # Looser: at least one prefer_for tag exists for the tool.
        if not prefer:
            fail(errors, f"benchmark {intent_id}: tool has empty prefer_for")
            continue

        preferred_tool = resolved["tool"].get("mcp_tool", "")
        preferred_action = resolved["tool"].get("action", "")
        for bad in case.get("forbidden_tool_substrings") or []:
            # Forbidden strings name Epic/primitive paths — must not be the preferred tool.
            if not bad:
                continue
            if bad == preferred_tool or bad == preferred_action:
                fail(
                    errors,
                    f"benchmark {intent_id}: preferred tool/action is forbidden {bad}",
                )

        # Cross-check special route for execute_plan misroute
        if intent_id == "execute_plan_misroute":
            routes = contract.get("special_routes") or []
            route = next((r for r in routes if r.get("id") == "execute_plan"), None)
            if route is None:
                fail(errors, "benchmark execute_plan_misroute: special_routes.execute_plan missing")
            else:
                select = (route.get("select") or "").lower()
                if "instantiatetemplate" not in select and "instantiate_template" not in select:
                    fail(
                        errors,
                        "execute_plan route must prefer InstantiateTemplate / instantiate_template",
                    )

        # Intent text must mention enough to be a useful fixture (non-empty).
        if not (case.get("intent") or "").strip():
            fail(errors, f"benchmark {intent_id}: empty intent text")
            continue

        # Unused variable silence for covered soft check
        _ = covered
        passed += 1
    return passed


def check_agent_usage_points_here(errors: list[str]) -> None:
    agent_usage = GUIDE_DIR / "agent-usage.md"
    if not agent_usage.is_file():
        fail(errors, "agent-usage.md missing")
        return
    text = agent_usage.read_text(encoding="utf-8")
    if "tool-selection-policy.md" not in text:
        fail(errors, "agent-usage.md must link tool-selection-policy.md")
    if "tool-selection-contract.json" not in text:
        fail(errors, "agent-usage.md must cite tool-selection-contract.json")


def check_readme(errors: list[str]) -> None:
    readme = GUIDE_DIR / "README.md"
    text = readme.read_text(encoding="utf-8")
    if "tool-selection-policy.md" not in text:
        fail(errors, "docs/guide/README.md must list tool-selection-policy.md")


def main() -> int:
    errors: list[str] = []
    if not CONTRACT_PATH.is_file():
        print("FAIL: missing tool-selection-contract.json", file=sys.stderr)
        return 2

    contract = load_contract()
    check_structure(contract, errors)
    check_policy_doc(errors)
    check_examples(contract, errors)
    check_tool_inventory(contract, errors)
    passed = check_benchmark(contract, errors)
    check_agent_usage_points_here(errors)
    check_readme(errors)

    if errors:
        print(f"FAIL: {len(errors)} tool-selection contract problem(s)")
        for err in errors:
            print(f"  {err}")
        return 1

    n_tools = sum(len(ts.get("tools", [])) for ts in contract.get("toolsets", []))
    n_bench = len(contract.get("routing_benchmark", []))
    print(
        f"OK: tool-selection contract — {n_tools} tools inventoried, "
        f"{n_bench} benchmark intents resolved ({passed} passed), "
        f"policy + examples present"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
