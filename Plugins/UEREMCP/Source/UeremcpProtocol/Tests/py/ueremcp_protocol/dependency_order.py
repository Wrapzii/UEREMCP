"""Batch dependency topological sort — mirrors FUeremcpDependencyOrder."""

from __future__ import annotations

from collections import deque
from typing import Iterable, Sequence


class DependencyError(ValueError):
    pass


def topological_sort(
    nodes: Sequence[dict],
) -> list[str]:
    """Sort operations by depends_on.

    Each node is ``{"id": str, "depends_on": [str, ...]}``.
    Preserves input order among ready nodes (stable Kahn).
    Raises DependencyError on duplicates, missing deps, or cycles.
    """
    ids: list[str] = []
    index: dict[str, int] = {}
    for i, node in enumerate(nodes):
        oid = node.get("id", "")
        if not oid:
            raise DependencyError("dependency node has empty id")
        if oid in index:
            raise DependencyError(f"duplicate operation id '{oid}'")
        index[oid] = i
        ids.append(oid)

    in_degree = {oid: 0 for oid in ids}
    dependents: dict[str, list[str]] = {oid: [] for oid in ids}

    for node in nodes:
        oid = node["id"]
        seen: set[str] = set()
        for dep in node.get("depends_on") or []:
            if not dep:
                raise DependencyError(f"operation '{oid}' has empty depends_on entry")
            if dep not in index:
                raise DependencyError(
                    f"operation '{oid}' depends_on unknown id '{dep}'"
                )
            if dep in seen:
                continue
            seen.add(dep)
            in_degree[oid] += 1
            dependents[dep].append(oid)

    ready = deque([oid for oid in ids if in_degree[oid] == 0])
    ordered: list[str] = []
    while ready:
        oid = ready.popleft()
        ordered.append(oid)
        for kid in dependents[oid]:
            in_degree[kid] -= 1
            if in_degree[kid] == 0:
                ready.append(kid)

    if len(ordered) != len(ids):
        cycle = [oid for oid in ids if in_degree[oid] > 0]
        raise DependencyError(
            "dependency cycle involving: " + ", ".join(cycle)
        )
    return ordered
