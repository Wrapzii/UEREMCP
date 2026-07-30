"""Provisional $ref resolution — mirrors FUeremcpRefResolve.

Grammar is provisional pending WS-02 audit. See plan.schema.json $comment and
docs/proposals/ws-05-batch-grammar-blocked.md.
"""

from __future__ import annotations

import copy
from typing import Any


class RefResolveError(ValueError):
    pass


def is_ref_object(value: Any) -> bool:
    return (
        isinstance(value, dict)
        and set(value.keys()) == {"$ref"}
        and isinstance(value["$ref"], str)
    )


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

    if isinstance(value, dict):
        return {k: _resolve(v, completed) for k, v in value.items()}
    if isinstance(value, list):
        return [_resolve(v, completed) for v in value]
    return value


def resolve_refs(specification: Any, completed_results: dict[str, Any]) -> Any:
    """Return a deep-copied specification with $ref objects substituted.

    Never substitutes null on failure — raises RefResolveError instead.
    """
    working = copy.deepcopy(specification)
    return _resolve(working, completed_results)
