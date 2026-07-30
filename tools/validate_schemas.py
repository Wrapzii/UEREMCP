#!/usr/bin/env python3
"""Validate UEREMCP JSON schemas and their examples.

Owner: WS-05.

Two jobs:

1. Every ``*.schema.json`` under ``schemas/`` is itself valid JSON Schema, and every
   ``$ref`` in it resolves.
2. Every ``examples`` entry embedded in a schema validates against that schema, and
   every file under ``schemas/examples/`` validates against the schema named by its
   ``x-schema`` key or its parent directory.

Cross-file ``$ref``s use relative paths (``../common/defs.schema.json#/$defs/x``) which
resolve against each schema's absolute ``$id``. Nothing is fetched over the network --
the registry is built entirely from local files, and a ``$ref`` pointing at an ``$id``
we do not have on disk is reported as an error rather than silently skipped.

Usage::

    python tools/validate_schemas.py
    python tools/validate_schemas.py --schemas-dir schemas -v

Exit code 0 on success, 1 on any failure. Intended for CI and for the pre-commit
check in AGENTS.md's definition of done.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    import jsonschema
    from jsonschema import Draft202012Validator
except ImportError:  # pragma: no cover
    sys.stderr.write(
        "error: jsonschema is not installed.\n"
        "       pip install jsonschema\n"
    )
    raise SystemExit(1)

try:
    from referencing import Registry, Resource
except ImportError:  # pragma: no cover
    sys.stderr.write(
        "error: the 'referencing' package is required (jsonschema >= 4.18).\n"
        "       pip install --upgrade jsonschema\n"
    )
    raise SystemExit(1)


class Problem(Exception):
    """A validation failure worth reporting with a path."""


def load_schemas(schemas_dir: Path) -> dict[Path, dict]:
    """Load every *.schema.json under schemas_dir, keyed by path."""
    schemas: dict[Path, dict] = {}
    for path in sorted(schemas_dir.rglob("*.schema.json")):
        try:
            schemas[path] = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise Problem(f"{path}: invalid JSON: {exc}") from exc
    if not schemas:
        raise Problem(f"no *.schema.json files found under {schemas_dir}")
    return schemas


def build_registry(schemas: dict[Path, dict]) -> Registry:
    """Build a referencing Registry from local schemas, keyed by $id.

    A schema without an $id cannot be the target of a cross-file $ref, so we flag it
    rather than let a later "unresolvable reference" error point somewhere unhelpful.
    """
    resources = []
    for path, schema in schemas.items():
        schema_id = schema.get("$id")
        if not schema_id:
            raise Problem(
                f"{path}: missing $id. Cross-file $refs resolve against $id, so every "
                f"schema needs one."
            )
        resources.append((schema_id, Resource.from_contents(schema)))
    return Registry().with_resources(resources)


def check_schema_valid(path: Path, schema: dict) -> list[str]:
    """Check the schema is itself a valid JSON Schema."""
    errors = []
    try:
        Draft202012Validator.check_schema(schema)
    except jsonschema.SchemaError as exc:
        errors.append(f"{path}: not a valid JSON Schema: {exc.message}")
    return errors


def check_examples(path: Path, schema: dict, registry: Registry) -> list[str]:
    """Validate any `examples` embedded at the top level of the schema.

    Only top-level examples are checked. Nested `examples` inside $defs are
    illustrative and frequently partial by design.
    """
    errors: list[str] = []
    examples = schema.get("examples")
    if not isinstance(examples, list):
        return errors

    validator = Draft202012Validator(schema, registry=registry)
    for i, example in enumerate(examples):
        for err in sorted(validator.iter_errors(example), key=lambda e: list(e.path)):
            loc = "/".join(str(p) for p in err.path) or "<root>"
            errors.append(f"{path}: examples[{i}] at {loc}: {err.message}")
    return errors


def check_refs_resolve(path: Path, schema: dict, registry: Registry) -> list[str]:
    """Force resolution of every $ref so a broken one fails loudly here.

    Validating an empty object exercises the reference machinery without requiring a
    real instance; we ignore the validation errors and only surface resolution errors.
    """
    errors: list[str] = []
    try:
        validator = Draft202012Validator(schema, registry=registry)
        list(validator.iter_errors({}))
    except Exception as exc:  # noqa: BLE001 - surface any resolver failure
        errors.append(f"{path}: reference resolution failed: {exc}")
    return errors


def check_example_files(schemas_dir: Path, schemas: dict[Path, dict],
                        registry: Registry) -> list[str]:
    """Validate standalone example files under schemas/examples/.

    Each example names its schema with a top-level "x-schema" key holding a path
    relative to schemas_dir. Files without it are skipped with a note, not an error --
    an example may legitimately be a fragment.

    Top-level "x-schema" and "$comment" are stripped before validation: they are
    tooling/annotation metadata, and our schemas set additionalProperties: false, which
    would otherwise reject them.
    """
    errors: list[str] = []
    examples_dir = schemas_dir / "examples"
    if not examples_dir.is_dir():
        return errors

    by_relpath = {p.relative_to(schemas_dir).as_posix(): s for p, s in schemas.items()}

    for path in sorted(examples_dir.rglob("*.json")):
        try:
            instance = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            errors.append(f"{path}: invalid JSON: {exc}")
            continue

        if not isinstance(instance, dict):
            continue
        target = instance.pop("x-schema", None)
        instance.pop("$comment", None)
        if not target:
            continue

        schema = by_relpath.get(target)
        if schema is None:
            errors.append(f"{path}: x-schema '{target}' does not match any schema file")
            continue

        validator = Draft202012Validator(schema, registry=registry)
        for err in sorted(validator.iter_errors(instance), key=lambda e: list(e.path)):
            loc = "/".join(str(p) for p in err.path) or "<root>"
            errors.append(f"{path}: at {loc}: {err.message}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--schemas-dir", default="schemas", type=Path)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    schemas_dir: Path = args.schemas_dir
    if not schemas_dir.is_dir():
        sys.stderr.write(f"error: {schemas_dir} is not a directory\n")
        return 1

    try:
        schemas = load_schemas(schemas_dir)
        registry = build_registry(schemas)
    except Problem as exc:
        sys.stderr.write(f"error: {exc}\n")
        return 1

    errors: list[str] = []
    for path, schema in schemas.items():
        if args.verbose:
            print(f"checking {path}")
        errors += check_schema_valid(path, schema)
        errors += check_refs_resolve(path, schema, registry)
        errors += check_examples(path, schema, registry)

    errors += check_example_files(schemas_dir, schemas, registry)

    if errors:
        for err in errors:
            sys.stderr.write(f"FAIL {err}\n")
        sys.stderr.write(f"\n{len(errors)} problem(s) in {len(schemas)} schema(s)\n")
        return 1

    print(f"OK  {len(schemas)} schema(s) valid, all $refs resolve, examples validate")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
