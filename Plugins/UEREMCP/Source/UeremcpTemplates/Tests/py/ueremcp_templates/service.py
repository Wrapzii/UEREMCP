"""Search + instantiate helpers mirroring FUeremcpTemplateService (WS-15)."""

from __future__ import annotations

import copy
import re
from dataclasses import dataclass
from typing import Any

from .store import TemplateRecord, TemplateStore

INPUT_REF = re.compile(r"^\{\{inputs\.([a-zA-Z0-9_]+)\}\}$")


@dataclass
class SearchQuery:
    query: str = ""
    domain: str = ""
    element: str = ""
    limit: int = 20


@dataclass
class SearchHit:
    template_id: str
    domain: str
    category: str
    description: str
    score: float


@dataclass
class InstantiateRequest:
    template_id: str
    inputs: dict[str, Any] | None = None
    modifiers: dict[str, list[str]] | None = None


@dataclass
class InstantiateResult:
    success: bool
    status: str
    summary: str
    plan: dict[str, Any] | None = None
    capability_notes: list[str] | None = None


class TemplateService:
    def __init__(self, store: TemplateStore) -> None:
        self._store = store

    def search(self, query: SearchQuery) -> list[SearchHit]:
        hits: list[SearchHit] = []
        for template_id in self._store.all_ids():
            record = self._store.find_by_id(template_id)
            if record is None:
                continue
            if query.domain and record.domain.lower() != query.domain.lower():
                continue
            if not _passes_element_filter(record, query.element):
                continue
            score = _score_record(record, query.query)
            if query.query and score <= 0:
                continue
            hits.append(
                SearchHit(
                    template_id=record.template_id,
                    domain=record.domain,
                    category=record.category,
                    description=record.description,
                    score=score,
                )
            )

        hits.sort(key=lambda hit: (-hit.score, hit.template_id))
        limit = max(1, min(query.limit, 100))
        return hits[:limit]

    def instantiate(self, request: InstantiateRequest) -> InstantiateResult:
        record = self._store.find_by_id(request.template_id)
        if record is None:
            return InstantiateResult(
                success=False,
                status="failed_validation",
                summary=f"Unknown template_id '{request.template_id}'.",
            )

        if request.modifiers:
            for bucket in ("replace", "adjust", "add", "preserve"):
                for modifier in request.modifiers.get(bucket, []):
                    if modifier not in record.supported_modifiers:
                        return InstantiateResult(
                            success=False,
                            status="failed_validation",
                            summary=(
                                f"Unsupported modifier '{modifier}' for template "
                                f"'{request.template_id}'."
                            ),
                        )

        plan_steps = record.document.get("construction_plan")
        if not isinstance(plan_steps, list):
            return InstantiateResult(
                success=False,
                status="failed_validation",
                summary="Template has no construction_plan.",
            )

        materialized = _apply_inputs(plan_steps, request.inputs or {})
        plan = {
            "template_id": record.template_id,
            "operations": materialized,
            "resolved_inputs": request.inputs or {},
            "executor": "execute_plan",
            "status_note": "materialized_only_v1",
        }
        if request.modifiers:
            plan["applied_modifiers"] = request.modifiers

        return InstantiateResult(
            success=True,
            status="partially_completed",
            summary=(
                f"Materialized construction_plan for '{request.template_id}'. "
                "execute_plan delegation not wired in v1."
            ),
            plan=plan,
            capability_notes=[
                "instantiate_template v1 returns materialized plan only; execute_plan not invoked."
            ],
        )


def _score_record(record: TemplateRecord, query: str) -> float:
    if not query:
        return 1.0
    needle = query.lower()
    score = 0.0
    if needle in record.template_id.lower():
        score += 3.0
    if needle in record.description.lower():
        score += 2.0
    if needle in record.category.lower():
        score += 1.0
    for term in record.search_terms:
        if needle in term.lower():
            score += 1.5
    return score


def _passes_element_filter(record: TemplateRecord, element: str) -> bool:
    if not element or element.lower() == "any":
        return True
    if not record.declared_element:
        return True
    inputs = record.document.get("inputs", {})
    properties = inputs.get("properties", {}) if isinstance(inputs, dict) else {}
    element_schema = properties.get("element", {}) if isinstance(properties, dict) else {}
    enum_values = element_schema.get("enum", []) if isinstance(element_schema, dict) else []
    return element.lower() in {str(value).lower() for value in enum_values}


def _apply_inputs(value: Any, inputs: dict[str, Any]) -> Any:
    if isinstance(value, str):
        match = INPUT_REF.match(value)
        if match:
            return copy.deepcopy(inputs.get(match.group(1)))
        return value
    if isinstance(value, list):
        return [_apply_inputs(item, inputs) for item in value]
    if isinstance(value, dict):
        return {key: _apply_inputs(item, inputs) for key, item in value.items()}
    return value
