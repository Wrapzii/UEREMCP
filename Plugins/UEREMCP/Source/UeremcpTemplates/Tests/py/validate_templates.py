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


def check_referential_integrity(
    templates_dir: Path,
    root: Path,
) -> list[str]:
    errors: list[str] = []
    templates: dict[str, dict] = {}
    presets: dict[str, dict] = {}

    for path in sorted(templates_dir.rglob("*.json")):
        document = json.loads(path.read_text(encoding="utf-8"))
        rel = path.relative_to(root)
        if path.parent.name == "elements":
            preset_id = document.get("preset_id")
            element = document.get("element")
            if preset_id != path.stem:
                errors.append(f"{rel}: preset_id '{preset_id}' must match filename stem")
            if element and f"element.{element}.v1" != path.stem and not path.stem.startswith(
                f"element.{element}."
            ):
                errors.append(f"{rel}: element '{element}' does not match filename")
            if isinstance(element, str):
                presets[element] = document
            continue

        template_id = document.get("template_id")
        if template_id != path.stem:
            errors.append(f"{rel}: template_id '{template_id}' must match filename stem")
        if isinstance(template_id, str):
            templates[template_id] = document

    for template_id, document in templates.items():
        inherits_from = document.get("inherits_from")
        if inherits_from and inherits_from not in templates:
            errors.append(
                f"{template_id}: inherits_from '{inherits_from}' is not loaded"
            )
        for composed in document.get("composes", []):
            if composed not in templates:
                errors.append(f"{template_id}: composes entry '{composed}' is not loaded")

        for operation in document.get("construction_plan", []):
            if not isinstance(operation, dict):
                continue
            if operation.get("action") != "create_niagara_effect":
                continue
            specification = operation.get("specification", {})
            if not isinstance(specification, dict):
                continue
            components = specification.get("components", [])
            if not isinstance(components, list):
                continue
            composes = set(document.get("composes", []))
            if not composes:
                continue
            archetypes = {
                entry.get("archetype")
                for entry in components
                if isinstance(entry, dict) and entry.get("archetype")
            }
            missing = sorted(archetype for archetype in archetypes if archetype not in composes)
            for archetype in missing:
                errors.append(
                    f"{template_id}: construction_plan archetype '{archetype}' "
                    "is not listed in composes"
                )

    elemental = templates.get("niagara.projectile.elemental.v1")
    if elemental:
        enum_values = (
            elemental.get("inputs", {})
            .get("properties", {})
            .get("element", {})
            .get("enum", [])
        )
        if set(enum_values) != set(presets):
            errors.append(
                "niagara.projectile.elemental.v1 element enum does not match "
                f"templates/elements presets ({sorted(enum_values)} vs {sorted(presets)})"
            )

    return errors


def main() -> int:
    root = repo_root()
    schemas_dir = root / "schemas"
    templates_dir = root / "templates"
    template_schema_path = schemas_dir / "template-library" / "template.schema.json"
    template_schema = json.loads(template_schema_path.read_text(encoding="utf-8"))
    element_schema_path = schemas_dir / "domains" / "templates" / "element_preset.schema.json"
    element_schema = json.loads(element_schema_path.read_text(encoding="utf-8"))
    registry = load_registry(schemas_dir)
    template_validator = Draft202012Validator(template_schema, registry=registry)
    element_validator = Draft202012Validator(element_schema, registry=registry)

    errors: list[str] = []
    count = 0
    for path in sorted(templates_dir.rglob("*.json")):
        count += 1
        instance = json.loads(path.read_text(encoding="utf-8"))
        validator = element_validator if path.parent.name == "elements" else template_validator
        for err in sorted(validator.iter_errors(instance), key=lambda e: list(e.path)):
            loc = "/".join(str(p) for p in err.path) or "<root>"
            errors.append(f"{path.relative_to(root)} at {loc}: {err.message}")

    errors.extend(check_referential_integrity(templates_dir, root))

    if errors:
        for err in errors:
            sys.stderr.write(f"FAIL {err}\n")
        sys.stderr.write(f"\n{len(errors)} template validation problem(s)\n")
        return 1

    print(f"OK  {count} template and element preset document(s) validate")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
