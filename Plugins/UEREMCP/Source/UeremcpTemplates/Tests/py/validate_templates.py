#!/usr/bin/env python3
"""Validate repo templates/ against template.schema.json (WS-15)."""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    import jsonschema
    from jsonschema import Draft202012Validator
    from referencing import Registry, Resource
except ImportError:
    sys.stderr.write("error: pip install jsonschema\n")
    raise SystemExit(1)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[6]


def load_registry(schemas_dir: Path) -> Registry:
    resources = []
    for path in sorted(schemas_dir.rglob("*.schema.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        schema_id = schema.get("$id")
        if not schema_id:
            raise RuntimeError(f"{path}: missing $id")
        resources.append((schema_id, Resource.from_contents(schema)))
    return Registry().with_resources(resources)


def main() -> int:
    root = repo_root()
    schemas_dir = root / "schemas"
    templates_dir = root / "templates"
    template_schema_path = schemas_dir / "template-library" / "template.schema.json"
    template_schema = json.loads(template_schema_path.read_text(encoding="utf-8"))
    registry = load_registry(schemas_dir)
    validator = Draft202012Validator(template_schema, registry=registry)

    errors: list[str] = []
    count = 0
    for path in sorted(templates_dir.rglob("*.json")):
        count += 1
        instance = json.loads(path.read_text(encoding="utf-8"))
        for err in sorted(validator.iter_errors(instance), key=lambda e: list(e.path)):
            loc = "/".join(str(p) for p in err.path) or "<root>"
            errors.append(f"{path.relative_to(root)} at {loc}: {err.message}")

    if errors:
        for err in errors:
            sys.stderr.write(f"FAIL {err}\n")
        sys.stderr.write(f"\n{len(errors)} template validation problem(s)\n")
        return 1

    print(f"OK  {count} template(s) validate against template.schema.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
