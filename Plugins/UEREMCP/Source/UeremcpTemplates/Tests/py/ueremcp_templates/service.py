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
    inherited_facts: list[str] | None = None
    overridden_facts: list[str] | None = None
    expected_validation_checks: list[str] | None = None
    non_executable_validation_checks: list[str] | None = None


@dataclass
class PromotionRequest:
    source_asset: str
    base_template_id: str = ""
    proposed_template_id: str = ""
    description: str = ""
    quarantine: bool = True
    dry_run: bool = True


@dataclass
class PromotionResult:
    success: bool
    status: str
    summary: str
    proposed_template_id: str = ""
    quarantine_path: str = ""
    contract_gates: list[str] | None = None
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

        element = effective_inputs.get("element")
        if isinstance(element, str):
            preset = self._store.find_element_preset(element)
            if preset is None:
                return InstantiateResult(
                    success=False,
                    status="failed_validation",
                    summary=f"No element preset is loaded for '{element}'.",
                )
            effective_inputs["preset_material_parameters"] = copy.deepcopy(
                preset.material_parameter_overrides
            )
            niagara_parameters = copy.deepcopy(preset.niagara_parameters)
            for override in ("scale", "intensity"):
                if override in effective_inputs:
                    niagara_parameters[override] = copy.deepcopy(
                        effective_inputs[override]
                    )
            effective_inputs["preset_niagara_parameters"] = niagara_parameters

        effective_target_path = effective_inputs.get("target_path", "")
        if isinstance(effective_target_path, str) and effective_target_path:
            target_folder, _, target_name = effective_target_path.rpartition("/")
            effective_inputs.setdefault(
                "core_material_path", f"{target_folder}/MI_{target_name}_Core"
            )
            effective_inputs.setdefault(
                "trail_material_path", f"{target_folder}/MI_{target_name}_Trail"
            )

        materialized = _apply_inputs(plan_steps, effective_inputs)
        modifier_error, modifier_validations = _apply_modifiers(
            materialized,
            record,
            request.modifiers or {},
            effective_inputs,
        )
        if modifier_error:
            return InstantiateResult(
                success=False,
                status="failed_validation",
                summary=modifier_error,
            )

        for operation in materialized:
            operation_id = operation.get("id")
            operation_target = effective_inputs.get(f"{operation_id}_path")
            if isinstance(operation_target, str) and operation_target:
                operation["target"] = {"asset_path": operation_target}
        if materialized:
            terminal = materialized[-1]
            if effective_target_path:
                terminal["target"] = {"asset_path": effective_target_path}
            if request.mode:
                terminal["mode"] = request.mode

        materialized.extend(modifier_validations)
        expected_checks: list[str] = []
        non_executable_checks: list[str] = []
        for rule in record.document.get("validation_rules", []):
            check_id = f"template.{record.template_id}.{rule['rule_id']}"
            operation = rule.get("operation")
            if isinstance(operation, dict):
                materialized.append(_apply_inputs(operation, effective_inputs))
                expected_checks.append(check_id)
            else:
                non_executable_checks.append(check_id)
        graph_error = _validate_operation_graph(materialized)
        if graph_error:
            return InstantiateResult(
                success=False,
                status="failed_validation",
                summary=graph_error,
            )

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
        inherited_facts = [f"template_id={record.template_id}"]
        if record.composes:
            inherited_facts.append(f"composes={','.join(record.composes)}")
        inherited_facts.append(
            "materialized_operations="
            + ",".join(str(operation["id"]) for operation in materialized)
        )
        overridden_facts = [
            f"input={name}"
            for name in sorted(effective_inputs)
            if name not in {"preset_material_parameters", "preset_niagara_parameters"}
        ]
        for bucket in ("replace", "adjust", "add", "preserve"):
            overridden_facts.extend(
                f"modifier.{bucket}={modifier}"
                for modifier in (request.modifiers or {}).get(bucket, [])
            )

        return InstantiateResult(
            success=True,
            status="partially_completed",
            summary=f"Materialized an execute_plan specification for '{request.template_id}'.",
            plan=plan,
            inherited_facts=inherited_facts,
            overridden_facts=overridden_facts,
            expected_validation_checks=expected_checks,
            non_executable_validation_checks=non_executable_checks,
        )

    def plan_promotion(self, request: PromotionRequest) -> PromotionResult:
        if (
            not request.source_asset.startswith("/Game/")
            or ".." in request.source_asset
            or request.source_asset.endswith("/")
        ):
            return PromotionResult(
                success=False,
                status="failed_validation",
                summary="source_asset must be a non-traversing /Game/ asset path.",
            )

        base = None
        if request.base_template_id:
            base = self._store.find_by_id(request.base_template_id)
            if base is None:
                return PromotionResult(
                    success=False,
                    status="failed_validation",
                    summary=f"Unknown base_template_id '{request.base_template_id}'.",
                )

        proposed_id = request.proposed_template_id
        if not proposed_id:
            domain = base.domain if base else "assets"
            category = base.category if base else "promoted"
            proposed_id = (
                f"{domain.lower()}.{category.lower()}."
                f"{_slug_from_asset_path(request.source_asset)}.v1"
            )
        if not re.fullmatch(r"[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+\.v[0-9]+", proposed_id):
            return PromotionResult(
                success=False,
                status="failed_validation",
                summary=(
                    "proposed_template_id does not match the versioned template "
                    "id contract."
                ),
            )

        notes = [
            "No source asset was inspected: a domain-neutral complete-state "
            "retrieval dispatcher is not registered.",
            "No construction_plan was synthesized or validated, and no quarantine "
            "file or asset was written.",
            "Promotion remains preview-only until UeremcpSecurity authorizes the "
            "write and the generated template passes schema validation.",
        ]
        if not request.quarantine:
            notes.append(
                "quarantine=false was not honored: direct writes outside the agent "
                "quarantine require a human-reviewed repository path."
            )
        if not request.dry_run:
            notes.append(
                "dry_run=false was requested, but mutation was withheld because "
                "promotion contract gates are incomplete."
            )
        return PromotionResult(
            success=True,
            status="partially_completed",
            summary=(
                f"Validated promotion intent for '{request.source_asset}' as "
                f"'{proposed_id}'; no source inspection or write occurred."
            ),
            proposed_template_id=proposed_id,
            quarantine_path=f"/Game/__UeremcpTemplates/agent/{proposed_id}",
            contract_gates=[
                "template.promotion.complete_state_retrieval",
                "template.promotion.reproduction_plan_synthesis",
                "template.promotion.schema_validation",
                "template.promotion.security_write_gate",
                "template.promotion.quarantine_write",
            ],
            capability_notes=notes,
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


def _slug_from_asset_path(source_asset: str) -> str:
    source_name = source_asset.rsplit("/", 1)[-1].lower()
    slug = re.sub(r"[^a-z0-9]+", "_", source_name).strip("_")
    if not slug or not slug[0].isalpha():
        slug = f"asset_{slug}"
    return slug


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


def _merge_patch(target: dict[str, Any], patch: dict[str, Any]) -> None:
    for key, value in patch.items():
        if value is None:
            target.pop(key, None)
        elif isinstance(value, dict):
            child = target.get(key)
            if not isinstance(child, dict):
                child = {}
            else:
                child = copy.deepcopy(child)
            _merge_patch(child, value)
            target[key] = child
        else:
            target[key] = copy.deepcopy(value)


def _apply_modifiers(
    operations: list[dict[str, Any]],
    record: TemplateRecord,
    requested: dict[str, list[str]],
    inputs: dict[str, Any],
) -> tuple[str, list[dict[str, Any]]]:
    definitions = record.document.get("modifier_definitions", {})
    seen: set[str] = set()
    validation_operations: list[dict[str, Any]] = []

    def find_operation(operation_id: str) -> dict[str, Any] | None:
        return next(
            (operation for operation in operations if operation.get("id") == operation_id),
            None,
        )

    for bucket in ("replace", "adjust", "add", "preserve"):
        for modifier in requested.get(bucket, []):
            if modifier in seen:
                return f"Modifier '{modifier}' was requested more than once.", []
            seen.add(modifier)
            if modifier not in record.supported_modifiers:
                return (
                    f"Unsupported modifier '{modifier}' for template "
                    f"'{record.template_id}'."
                ), []
            definition = definitions.get(modifier)
            if not isinstance(definition, dict):
                return f"Modifier '{modifier}' is declared but has no executable delta.", []
            if definition.get("bucket") != bucket:
                return (
                    f"Modifier '{modifier}' belongs to bucket "
                    f"'{definition.get('bucket', '')}', not '{bucket}'."
                ), []

            for operation_id, replacement in definition.get(
                "replace_operations", {}
            ).items():
                existing = find_operation(operation_id)
                if existing is None:
                    return (
                        f"Modifier '{modifier}' replaces unknown operation "
                        f"'{operation_id}'."
                    ), []
                materialized_replacement = _apply_inputs(replacement, inputs)
                if materialized_replacement.get("id") != operation_id:
                    return (
                        f"Modifier '{modifier}' replacement must preserve operation "
                        f"id '{operation_id}'."
                    ), []
                operations[operations.index(existing)] = materialized_replacement

            for operation_id, patch in definition.get(
                "merge_specifications", {}
            ).items():
                operation = find_operation(operation_id)
                if operation is None:
                    return (
                        f"Modifier '{modifier}' patches unknown operation "
                        f"'{operation_id}'."
                    ), []
                specification = copy.deepcopy(operation.get("specification", {}))
                _merge_patch(specification, _apply_inputs(patch, inputs))
                operation["specification"] = specification

            operations.extend(
                _apply_inputs(definition.get("append_operations", []), inputs)
            )
            validation_operations.extend(
                _apply_inputs(definition.get("validation_operations", []), inputs)
            )

    return "", validation_operations


def _validate_operation_graph(operations: list[dict[str, Any]]) -> str:
    ids: set[str] = set()
    for operation in operations:
        operation_id = operation.get("id")
        action = operation.get("action")
        if not isinstance(operation_id, str) or not operation_id or not isinstance(
            action, str
        ) or not action:
            return "Materialized plan operation requires non-empty id and action."
        if operation_id in ids:
            return f"Duplicate materialized operation id '{operation_id}'."
        ids.add(operation_id)
    for operation in operations:
        for dependency in operation.get("depends_on", []):
            if dependency not in ids:
                return (
                    f"Operation '{operation['id']}' depends on unknown operation "
                    f"'{dependency}'."
                )
    return ""


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


def build_promotion_response(
    original_envelope: dict[str, Any],
    request: PromotionRequest,
    result: PromotionResult,
) -> dict[str, Any]:
    response: dict[str, Any] = {
        "protocol_version": "1.0",
        "request_id": original_envelope.get("request_id", ""),
        "status": result.status,
        "summary": result.summary,
        "understood": {"action": "promote_to_template"},
        "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
    }
    if result.proposed_template_id:
        response["understood"]["template_used"] = result.proposed_template_id
    if result.quarantine_path:
        response["understood"]["resolved_target"] = result.quarantine_path
    if result.success:
        response["understood"]["interpretation_notes"] = [
            (
                "Promotion defaulted to preview-only dry_run behavior."
                if request.dry_run
                else "Promotion mutation was requested but withheld behind contract gates."
            ),
            (
                "Resolved output to the agent quarantine."
                if request.quarantine
                else "Resolved a quarantine preview despite quarantine=false."
            ),
        ]
        response["validation"] = {
            "checks_skipped": list(result.contract_gates or [])
        }
    if result.capability_notes:
        response["capability_notes"] = list(result.capability_notes)
    return response


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
    understood.setdefault("interpretation_notes", []).extend(
        f"inherited:{fact}" for fact in materialized.inherited_facts or []
    )
    understood["interpretation_notes"].extend(
        f"overridden:{fact}" for fact in materialized.overridden_facts or []
    )

    validation = response.setdefault("validation", {})
    performed = set(validation.get("checks_performed", []))
    missing_checks = list(materialized.non_executable_validation_checks or [])
    missing_checks.extend(
        check
        for check in materialized.expected_validation_checks or []
        if check not in performed
    )
    if missing_checks:
        if response["status"] in {
            "created_and_validated",
            "modified_and_validated",
            "created_with_warnings",
            "no_change_required",
        }:
            response["status"] = "partially_completed"
        response.setdefault("capability_notes", []).append(
            "One or more template validation rules lacked re-read evidence; "
            "validated status was withheld."
        )
        validation.setdefault("checks_skipped", []).extend(missing_checks)
    return response
