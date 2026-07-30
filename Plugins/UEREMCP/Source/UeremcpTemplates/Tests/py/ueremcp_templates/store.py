"""Python mirror of FUeremcpTemplateStore for out-of-editor tests (WS-15)."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class TemplateRecord:
    template_id: str
    domain: str = ""
    category: str = ""
    version: int = 0
    description: str = ""
    search_terms: list[str] = field(default_factory=list)
    composes: list[str] = field(default_factory=list)
    inherits_from: str = ""
    supported_modifiers: list[str] = field(default_factory=list)
    declared_element: str = ""
    document: dict[str, Any] = field(default_factory=dict)
    source_path: str = ""


class TemplateStore:
    def __init__(self) -> None:
        self._records: dict[str, TemplateRecord] = {}

    def reset(self) -> None:
        self._records.clear()

    def load_from_directory(self, root: Path) -> tuple[int, list[str]]:
        self.reset()
        errors: list[str] = []
        if not root.is_dir():
            return 0, [f"template directory not found: {root}"]

        loaded = 0
        for path in sorted(root.rglob("*.json")):
            try:
                record = self._parse_file(path)
            except ValueError as exc:
                errors.append(f"{path}: {exc}")
                continue

            if record.template_id in self._records:
                errors.append(f"{path}: duplicate template_id '{record.template_id}'")
                continue

            self._records[record.template_id] = record
            loaded += 1

        return loaded, errors

    def find_by_id(self, template_id: str) -> TemplateRecord | None:
        return self._records.get(template_id)

    def all_ids(self) -> list[str]:
        return sorted(self._records)

    def count(self) -> int:
        return len(self._records)

    def _parse_file(self, path: Path) -> TemplateRecord:
        document = json.loads(path.read_text(encoding="utf-8"))
        template_id = document.get("template_id")
        if not template_id:
            raise ValueError("missing template_id")

        record = TemplateRecord(
            template_id=template_id,
            domain=document.get("domain", ""),
            category=document.get("category", ""),
            version=int(document.get("version", 0)),
            description=document.get("description", ""),
            search_terms=list(document.get("search_terms", [])),
            composes=list(document.get("composes", [])),
            inherits_from=document.get("inherits_from", ""),
            supported_modifiers=list(document.get("supported_modifiers", [])),
            document=document,
            source_path=str(path),
        )
        self._index_element(record)
        return record

    @staticmethod
    def _index_element(record: TemplateRecord) -> None:
        inputs = record.document.get("inputs", {})
        properties = inputs.get("properties", {}) if isinstance(inputs, dict) else {}
        if isinstance(properties, dict) and "element" in properties:
            record.declared_element = "parameterized"
