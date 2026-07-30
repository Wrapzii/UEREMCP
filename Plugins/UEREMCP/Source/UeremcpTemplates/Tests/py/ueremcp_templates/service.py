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
    target_asset_path: str = ""
    mode: str = "create_or_update"


@dataclass
class InstantiateResult:
    success: bool
    status: str
    summary: str
    plan: dict[str, Any] | None = None
    capability_notes: list[str] | None = None
    has_template_validation_rules: bool = False


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
                    return InstantiateResult(
                        success=False,
                        status="failed_validation",
                        summary=(
                            f"Modifier '{modifier}' is declared but has no executable "
                            "delta in the frozen template schema."
                        ),
                        capability_notes=[
                            "Named modifier execution is blocked until "
                            "template.schema.json can represent modifier deltas."
                        ],
                    )

        plan_steps = record.document.get("construction_plan")
        if not isinstance(plan_steps, list):
            return InstantiateResult(
                success=False,
                status="failed_validation",
                summary="Template has no construction_plan.",
            )

        effective_inputs = copy.deepcopy(request.inputs or {})
        if request.target_asset_path and "target_path" not in effective_inputs:
            effective_inputs["target_path"] = request.target_asset_path
        input_error = _validate_inputs(record.document.get("inputs"), effective_inputs)
        if input_error:
            return InstantiateResult(
                success=False,
                status="failed_validation",
                summary=input_error,
            )

        materialized = _apply_inputs(plan_steps, effective_inputs)
        if materialized:
            terminal = materialized[-1]
            if request.target_asset_path:
                terminal["target"] = {"asset_path": request.target_asset_path}
            if request.mode:
                terminal["mode"] = request.mode
        plan = {
            "transaction": {
                "atomic": True,
                "rollback_on_failure": True,
                "compile_policy": "at_boundaries",
                "validate_policy": "at_end",
            },
            "operations": materialized,
            "on_failure": "rollback_all",
        }

        return InstantiateResult(
            success=True,
            status="partially_completed",
            summary=f"Materialized an execute_plan specification for '{request.template_id}'.",
            plan=plan,
            has_template_validation_rules=bool(record.document.get("validation_rules")),
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


def _validate_inputs(schema: Any, inputs: dict[str, Any]) -> str:
    if not isinstance(schema, dict):
        return ""
    for name in schema.get("required", []):
        if name not in inputs:
            return f"Missing required template input '{name}'."
    properties = schema.get("properties", {})
    if not isinstance(properties, dict):
        return ""
    type_checks = {
        "string": lambda value: isinstance(value, str),
        "number": lambda value: isinstance(value, (int, float)) and not isinstance(value, bool),
        "integer": lambda value: isinstance(value, int) and not isinstance(value, bool),
        "boolean": lambda value: isinstance(value, bool),
        "object": lambda value: isinstance(value, dict),
        "array": lambda value: isinstance(value, list),
    }
    for name, value in inputs.items():
        property_schema = properties.get(name)
        if not isinstance(property_schema, dict):
            continue
        expected_type = property_schema.get("type")
        if expected_type in type_checks and not type_checks[expected_type](value):
            return f"Template input '{name}' must be {expected_type}."
        if "enum" in property_schema and value not in property_schema["enum"]:
            return f"Template input '{name}' is not an allowed value."
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            if "minimum" in property_schema and value < property_schema["minimum"]:
                return f"Template input '{name}' is below its minimum."
            if "maximum" in property_schema and value > property_schema["maximum"]:
                return f"Template input '{name}' is above its maximum."
    return ""


def delegate_execute_plan(
    original_envelope: dict[str, Any],
    request: InstantiateRequest,
    materialized: InstantiateResult,
    executor: Any,
) -> dict[str, Any]:
    if not materialized.success or materialized.plan is None:
        raise ValueError("cannot delegate an invalid template materialization")
    execute_request = copy.deepcopy(original_envelope)
    execute_request["action"] = "execute_plan"
    execute_request["specification"] = copy.deepcopy(materialized.plan)
    response = executor(execute_request)
    if not isinstance(response, dict):
        raise ValueError("execute_plan returned invalid JSON")
    if not isinstance(response.get("status"), str):
        raise ValueError("execute_plan response is missing status")
    if not isinstance(response.get("summary"), str):
        raise ValueError("execute_plan response is missing summary")
    metrics = response.get("metrics")
    if not isinstance(metrics, dict) or not {
        "mcp_round_trips",
        "internal_operations",
    }.issubset(metrics):
        raise ValueError("execute_plan response is missing metrics")

    response = copy.deepcopy(response)
    response["request_id"] = original_envelope.get("request_id", "")
    response["summary"] = (
        f"Instantiated template '{request.template_id}' through execute_plan. "
        f"{response['summary']}"
    )
    response["metrics"]["mcp_round_trips"] = 1
    understood = response.setdefault("understood", {})
    understood["action"] = "instantiate_template"
    understood["template_used"] = request.template_id
    if request.target_asset_path:
        understood["resolved_target"] = request.target_asset_path

    if materialized.has_template_validation_rules:
        if response["status"] in {
            "created_and_validated",
            "modified_and_validated",
            "created_with_warnings",
            "no_change_required",
        }:
            response["status"] = "partially_completed"
        response.setdefault("capability_notes", []).append(
            "Template validation_rules were not executed: execute_plan has no "
            "registered template-rule post-step contract."
        )
        response.setdefault("validation", {}).setdefault("checks_skipped", []).append(
            "template.validation_rules"
        )
    return response
