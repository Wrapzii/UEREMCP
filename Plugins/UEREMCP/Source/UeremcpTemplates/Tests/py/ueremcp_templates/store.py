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


@dataclass
class ElementPreset:
    preset_id: str
    element: str
    version: int
    material_parameter_overrides: dict[str, Any]
    niagara_parameters: dict[str, Any]
    source_path: str = ""


class TemplateStore:
    def __init__(self) -> None:
        self._records: dict[str, TemplateRecord] = {}
        self._element_presets: dict[str, ElementPreset] = {}

    def reset(self) -> None:
        self._records.clear()
        self._element_presets.clear()

    def load_from_directory(self, root: Path) -> tuple[int, list[str]]:
        self.reset()
        errors: list[str] = []
        if not root.is_dir():
            return 0, [f"template directory not found: {root}"]

        loaded = 0
        for path in sorted(root.rglob("*.json")):
            if path.parent.name.lower() == "elements":
                try:
                    preset = self._parse_element_preset(path)
                except ValueError as exc:
                    errors.append(f"{path}: {exc}")
                    continue
                key = preset.element.lower()
                if key in self._element_presets:
                    errors.append(f"{path}: duplicate element preset '{preset.element}'")
                    continue
                self._element_presets[key] = preset
                continue

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

    def find_element_preset(self, element: str) -> ElementPreset | None:
        return self._element_presets.get(element.lower())

    def count(self) -> int:
        return len(self._records)

    def element_preset_count(self) -> int:
        return len(self._element_presets)

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
    def _parse_element_preset(path: Path) -> ElementPreset:
        document = json.loads(path.read_text(encoding="utf-8"))
        try:
            return ElementPreset(
                preset_id=document["preset_id"],
                element=document["element"],
                version=int(document["version"]),
                material_parameter_overrides=document["material"]["parameter_overrides"],
                niagara_parameters=document["niagara"]["parameters"],
                source_path=str(path),
            )
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError("invalid element preset") from exc

    @staticmethod
    def _index_element(record: TemplateRecord) -> None:
        inputs = record.document.get("inputs", {})
        properties = inputs.get("properties", {}) if isinstance(inputs, dict) else {}
        if isinstance(properties, dict) and "element" in properties:
            record.declared_element = "parameterized"
