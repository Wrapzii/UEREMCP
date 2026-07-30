"""Batch $ref resolution — mirrors FUeremcpRefResolve.

Final grammar (docs/proposals/ws-05-batch-ref-grammar.md):
  1. Object form {"$ref": "op.path..."} — canonical
  2. Dollar-string "$op_id" — REAgentTools prior art
     [VERIFIED: batch_workflow_tools.py:37-48]
"""

from __future__ import annotations

import copy
import re
from typing import Any

_DOLLAR_RE = re.compile(r"^\$[a-zA-Z0-9_-]+$")


class RefResolveError(ValueError):
    pass


def is_ref_object(value: Any) -> bool:
    return (
        isinstance(value, dict)
        and set(value.keys()) == {"$ref"}
        and isinstance(value["$ref"], str)
    )


def is_dollar_string_ref(value: Any) -> bool:
    return isinstance(value, str) and bool(_DOLLAR_RE.match(value))


def parse_ref(ref: str) -> tuple[str, list[str]]:
    if not ref:
        raise RefResolveError("$ref string is empty")
    parts = ref.split(".")
    if len(parts) < 2 or any(p == "" for p in parts):
        raise RefResolveError(
            f"$ref '{ref}' must be '<operation_id>.<dotted.path>'"
        )
    return parts[0], parts[1:]


def lookup_path(root: Any, path: list[str]) -> Any:
    current = root
    for segment in path:
        if isinstance(current, dict):
            if segment not in current:
                raise RefResolveError(f"missing field '{segment}'")
            current = current[segment]
        elif isinstance(current, list):
            if not segment.isdigit():
                raise RefResolveError(f"array index '{segment}' is not numeric")
            idx = int(segment)
            if idx < 0 or idx >= len(current):
                raise RefResolveError(f"array index {idx} out of range")
            current = current[idx]
        else:
            raise RefResolveError(
                f"cannot traverse into non-container at segment '{segment}'"
            )
    return current


def resolve_dollar_shorthand(operation_id: str, completed: Any) -> str:
    """result.primary_asset → label → path. Fail if none.

    [VERIFIED: batch_workflow_tools.py:37-48] for label/path order on step bags;
    primary_asset is the UEREMCP envelope equivalent of that primary identity.
    """
    if not isinstance(completed, dict):
        raise RefResolveError(f"${operation_id}: completed result is not an object")

    result = completed.get("result")
    if isinstance(result, dict):
        primary = result.get("primary_asset")
        if isinstance(primary, str) and primary:
            return primary

    for key in ("label", "path"):
        val = completed.get(key)
        if isinstance(val, str) and val:
            return val

    raise RefResolveError(
        f"${operation_id} has no result.primary_asset, label, or path"
    )


def _resolve(value: Any, completed: dict[str, Any]) -> Any:
    if is_ref_object(value):
        ref = value["$ref"]
        op_id, path = parse_ref(ref)
        if op_id not in completed:
            raise RefResolveError(
                f"$ref '{ref}': operation '{op_id}' has no completed result"
            )
        try:
            return lookup_path(completed[op_id], path)
        except RefResolveError as exc:
            raise RefResolveError(f"$ref '{ref}': {exc}") from exc

    if is_dollar_string_ref(value):
        op_id = value[1:]
        if op_id not in completed:
            raise RefResolveError(f"${op_id}: operation has no completed result")
        return resolve_dollar_shorthand(op_id, completed[op_id])

    if isinstance(value, dict):
        return {k: _resolve(v, completed) for k, v in value.items()}
    if isinstance(value, list):
        return [_resolve(v, completed) for v in value]
    return value


def resolve_refs(specification: Any, completed_results: dict[str, Any]) -> Any:
    """Deep-copy specification with both $ref forms substituted.

    Never substitutes null on failure — raises RefResolveError instead.
    """
    working = copy.deepcopy(specification)
    return _resolve(working, completed_results)
