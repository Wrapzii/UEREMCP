"""Load local UEREMCP JSON Schema registry (offline, no network)."""

from __future__ import annotations

import json
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Registry, Resource


def repo_root() -> Path:
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "schemas" / "graph" / "graph.schema.json").is_file():
            return parent
    raise RuntimeError("could not locate repo root (schemas/graph/graph.schema.json)")


def load_schema_registry(schemas_dir: Path | None = None) -> Registry:
    root = repo_root() if schemas_dir is None else schemas_dir.parent
    schemas_path = schemas_dir or (root / "schemas")
    resources: list[tuple[str, Resource]] = []
    for path in sorted(schemas_path.rglob("*.schema.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        schema_id = schema.get("$id")
        if not schema_id:
            raise ValueError(f"{path}: missing $id")
        resources.append((schema_id, Resource.from_contents(schema)))
    return Registry().with_resources(resources)


def graph_schema_validator(registry: Registry | None = None) -> Draft202012Validator:
    reg = registry or load_schema_registry()
    graph_path = repo_root() / "schemas" / "graph" / "graph.schema.json"
    graph_schema = json.loads(graph_path.read_text(encoding="utf-8"))
    return Draft202012Validator(graph_schema, registry=reg)
