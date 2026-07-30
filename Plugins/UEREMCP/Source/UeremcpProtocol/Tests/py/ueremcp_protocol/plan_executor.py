"""Offline execute_plan interpreter — mirrors FUeremcpPlanExecutor.

Authority: schemas/batch/plan.schema.json and Docs/PLAN_EXECUTOR.md.
This is a regression mirror for outside-editor tests; C++ is the editor
runtime. Keep semantics aligned with Private/UeremcpPlanExecutor.cpp.
"""

from __future__ import annotations

import copy
import json
import re
import threading
from typing import Any, Callable

from .dependency_order import DependencyError, topological_sort
from .envelope import PROTOCOL_VERSION, STATUSES, parse_request
from .ref_resolve import RefResolveError, resolve_refs

_ACTION_RE = re.compile(r"^[a-z][a-z0-9_]*$")
_SUCCESS = frozenset(
    {
        "created_and_validated",
        "modified_and_validated",
        "created_with_warnings",
        "no_change_required",
    }
)
_COMPILE = frozenset({"per_operation", "at_boundaries", "at_end", "never"})
_VALIDATE = frozenset({"per_operation", "at_boundaries", "at_end"})
_ON_FAILURE = frozenset({"stop", "continue_independent", "rollback_all"})

Handler = Callable[[str], tuple[bool, str, str]]
# handler(request_json) -> (ok, response_json, error)
TxnFn = Callable[[], tuple[bool, str]]


class PlanExecutorError(ValueError):
    pass


class PlanExecutor:
    """Process-local fail-closed execute_plan interpreter."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._handlers: dict[str, Handler] = {}
        self._begin: TxnFn | None = None
        self._commit: TxnFn | None = None
        self._rollback: TxnFn | None = None

    def clear(self) -> None:
        with self._lock:
            self._handlers.clear()
            self._begin = self._commit = self._rollback = None

    def register_action(self, action: str, handler: Handler) -> None:
        if not _ACTION_RE.match(action) or action == "execute_plan" or handler is None:
            raise PlanExecutorError(
                "plan action registration requires a valid non-recursive handler"
            )
        with self._lock:
            if action in self._handlers:
                raise PlanExecutorError(f"plan action '{action}' is already registered")
            self._handlers[action] = handler

    def unregister_action(self, action: str) -> None:
        with self._lock:
            self._handlers.pop(action, None)

    def set_transaction_callbacks(
        self, begin: TxnFn, commit: TxnFn, rollback: TxnFn
    ) -> None:
        if not (begin and commit and rollback):
            raise PlanExecutorError(
                "transaction callbacks require begin, commit, and rollback"
            )
        with self._lock:
            self._begin, self._commit, self._rollback = begin, commit, rollback

    def clear_transaction_callbacks(self) -> None:
        with self._lock:
            self._begin = self._commit = self._rollback = None

    def execute_request(self, request_json: str) -> tuple[bool, str]:
        """Return (success_flag_for_required_ops, response_json).

        Always returns a structured envelope. Structured rejections still yield
        ``True`` for the bool when no required mutation failed after begin —
        matching the C++ ``Return(...)`` contract used by automation tests.
        """
        try:
            parsed = parse_request(request_json)
        except Exception as exc:
            return True, _reject("", str(exc))

        if parsed["action"] != "execute_plan":
            return True, _reject(parsed.get("request_id", ""), "expected action execute_plan")

        root = json.loads(request_json)
        request_id = parsed.get("request_id") or ""
        specification = parsed.get("specification")
        if not isinstance(specification, dict):
            return True, _reject(request_id, "execute_plan specification.operations must be a non-empty array")

        try:
            operations, order = _parse_operations(specification)
            atomic, rollback_on_failure, compile_policy, validate_policy, on_failure = (
                _parse_policies(specification)
            )
        except PlanExecutorError as exc:
            return True, _reject(request_id, str(exc))

        with self._lock:
            handlers = dict(self._handlers)
            begin, commit, rollback = self._begin, self._commit, self._rollback

        for op in operations:
            if op["action"] not in handlers:
                return True, _reject(
                    request_id, f"no handler registered for '{op['action']}'"
                )
        if atomic and not (begin and commit and rollback):
            return True, _reject(
                request_id, "atomic execute_plan requires transaction callbacks"
            )

        txn_open = False
        if atomic:
            ok, err = begin()
            if not ok:
                return True, _reject(request_id, err or "transaction begin failed")
            txn_open = True

        by_id = {op["id"]: op for op in operations}
        completed: dict[str, dict[str, Any]] = {}
        status_by_id: dict[str, str] = {}
        results: list[dict[str, Any]] = []
        successful_list: list[dict[str, Any]] = []
        internal_ops = 0
        required_failure = False
        any_failure = False
        stop = False

        for index, op_id in enumerate(order):
            op = by_id[op_id]
            skip_reason = ""
            for dep in op["depends_on"]:
                dep_status = status_by_id.get(dep)
                if dep_status not in _SUCCESS:
                    skip_reason = (
                        f"dependency '{dep}' did not complete successfully"
                    )
                    break
            if stop or skip_reason:
                if stop:
                    skip_reason = "plan stopped after an earlier required failure"
                results.append(
                    _op_result(
                        op,
                        "partially_completed",
                        "Operation was not executed.",
                        skip_reason,
                    )
                )
                status_by_id[op_id] = "partially_completed"
                any_failure = True
                required_failure = required_failure or not op["optional"]
                continue

            should_run = True
            try:
                should_run, skip_reason = _condition_allows(op, status_by_id)
            except PlanExecutorError as exc:
                should_run = False
                skip_reason = str(exc)
                any_failure = True
            if not should_run:
                results.append(
                    _op_result(
                        op,
                        "no_change_required",
                        "Condition not satisfied.",
                        skip_reason,
                    )
                )
                status_by_id[op_id] = "no_change_required"
                continue

            compile_flag = _should_compile(
                compile_policy, op, operations, index, len(order)
            )
            validate_flag = (
                validate_policy == "per_operation"
                or (validate_policy == "at_boundaries" and compile_flag)
                or (validate_policy == "at_end" and index == len(order) - 1)
            )
            status = "error"
            summary = "semantic handler failed"
            response: dict[str, Any] | None = None
            try:
                nested = _build_nested_request(
                    root, op, completed, compile_flag, validate_flag
                )
                ok, response_json, err = handlers[op["action"]](json.dumps(nested))
                if not ok:
                    summary = err or "semantic handler failed"
                else:
                    response = json.loads(response_json)
                    status = response.get("status", "")
                    summary = response.get("summary", "")
                    metrics = response.get("metrics") or {}
                    if (
                        status not in STATUSES
                        or not summary
                        or "internal_operations" not in metrics
                    ):
                        status = "error"
                        summary = (
                            "semantic handler returned an invalid response envelope"
                        )
                    else:
                        internal_ops += max(0, int(metrics["internal_operations"]))
            except (RefResolveError, PlanExecutorError, json.JSONDecodeError, TypeError, ValueError) as exc:
                summary = str(exc)

            results.append(_op_result(op, status, summary))
            status_by_id[op_id] = status
            if status in _SUCCESS and response is not None:
                completed[op_id] = response
                successful_list.append(response)
            else:
                any_failure = True
                required_failure = required_failure or not op["optional"]
                stop = (not op["optional"]) and on_failure != "continue_independent"

        rolled_back = False
        if txn_open:
            if required_failure and (rollback_on_failure or on_failure == "rollback_all"):
                rolled_back, _ = rollback()
            else:
                ok, err = commit()
                if not ok:
                    any_failure = True
                    required_failure = True
                    rolled_back, _ = rollback()

        if rolled_back:
            final_status = "rolled_back"
            summary = "execute_plan failed and rolled back."
        elif any_failure:
            final_status = "partially_completed"
            summary = "execute_plan completed only an independent subset."
        elif not successful_list:
            final_status = "no_change_required"
            summary = f"execute_plan completed {len(operations)} operation(s)."
        else:
            final_status = successful_list[-1].get("status", "no_change_required")
            summary = f"execute_plan completed {len(operations)} operation(s)."

        if rolled_back or not successful_list:
            final: dict[str, Any] = {
                "protocol_version": PROTOCOL_VERSION,
                "request_id": request_id,
                "status": final_status,
                "summary": summary,
                "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
            }
        else:
            final = copy.deepcopy(successful_list[-1])
            final["protocol_version"] = PROTOCOL_VERSION
            final["request_id"] = request_id
            final["status"] = final_status
            final["summary"] = summary

        understood = copy.deepcopy(final.get("understood") or {})
        understood["action"] = "execute_plan"
        final["understood"] = understood

        aggregate: dict[str, Any] = {"operations": results}
        if not rolled_back:
            for field in (
                "created_assets",
                "modified_assets",
                "deleted_assets",
                "reused_assets",
                "dependencies",
                "unresolved_dependencies",
            ):
                values: list[Any] = []
                for resp in successful_list:
                    values.extend((resp.get("result") or {}).get(field) or [])
                if values:
                    aggregate[field] = values
            if successful_list:
                primary = (successful_list[-1].get("result") or {}).get("primary_asset")
                if primary:
                    aggregate["primary_asset"] = primary
        final["result"] = aggregate

        if not rolled_back:
            changes: list[Any] = []
            for resp in successful_list:
                changes.extend(resp.get("changes") or [])
            if changes:
                final["changes"] = changes

        final["metrics"] = {
            "mcp_round_trips": 1,
            "internal_operations": internal_ops,
        }
        if rolled_back:
            final["rollback"] = {
                "available": True,
                "performed": True,
                "scope": "full",
            }
            for key in ("revision", "validation"):
                final.pop(key, None)

        return True, json.dumps(final, ensure_ascii=False)


def _reject(request_id: str, reason: str) -> str:
    body: dict[str, Any] = {
        "protocol_version": PROTOCOL_VERSION,
        "status": "rejected",
        "summary": reason,
        "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
    }
    if request_id:
        body["request_id"] = request_id
    return json.dumps(body, ensure_ascii=False)


def _parse_operations(
    specification: dict[str, Any],
) -> tuple[list[dict[str, Any]], list[str]]:
    values = specification.get("operations")
    if not isinstance(values, list) or not values:
        raise PlanExecutorError(
            "execute_plan specification.operations must be a non-empty array"
        )
    operations: list[dict[str, Any]] = []
    nodes: list[dict[str, Any]] = []
    for value in values:
        if not isinstance(value, dict):
            raise PlanExecutorError("execute_plan operations must be objects")
        op_id = value.get("id")
        action = value.get("action")
        if not isinstance(op_id, str) or not op_id:
            raise PlanExecutorError(
                "every operation requires an id and valid non-recursive action"
            )
        if not isinstance(action, str) or not _ACTION_RE.match(action) or action == "execute_plan":
            raise PlanExecutorError(
                "every operation requires an id and valid non-recursive action"
            )
        depends = value.get("depends_on") or []
        if not isinstance(depends, list) or any(
            not isinstance(d, str) or not d for d in depends
        ):
            raise PlanExecutorError("depends_on entries must be non-empty strings")
        condition = value.get("condition")
        if isinstance(condition, dict) and (
            "asset_exists" in condition or "asset_missing" in condition
        ):
            raise PlanExecutorError(
                f"operation '{op_id}' uses an asset condition without an evaluator"
            )
        op = {
            "id": op_id,
            "action": action,
            "depends_on": list(depends),
            "optional": bool(value.get("optional", False)),
            "object": value,
        }
        operations.append(op)
        nodes.append({"id": op_id, "depends_on": list(depends)})
    try:
        order = topological_sort(nodes)
    except DependencyError as exc:
        raise PlanExecutorError(str(exc)) from exc
    return operations, order


def _parse_policies(
    specification: dict[str, Any],
) -> tuple[bool, bool, str, str, str]:
    atomic = True
    rollback_on_failure = True
    compile_policy = "at_boundaries"
    validate_policy = "at_end"
    txn = specification.get("transaction")
    if isinstance(txn, dict):
        atomic = bool(txn.get("atomic", True))
        rollback_on_failure = bool(txn.get("rollback_on_failure", True))
        compile_policy = txn.get("compile_policy", compile_policy)
        validate_policy = txn.get("validate_policy", validate_policy)
    on_failure = specification.get("on_failure", "rollback_all")
    if (
        compile_policy not in _COMPILE
        or validate_policy not in _VALIDATE
        or on_failure not in _ON_FAILURE
    ):
        raise PlanExecutorError("invalid plan policy")
    return atomic, rollback_on_failure, compile_policy, validate_policy, on_failure


def _should_compile(
    policy: str,
    op: dict[str, Any],
    operations: list[dict[str, Any]],
    index: int,
    count: int,
) -> bool:
    if policy == "never":
        return False
    if policy == "per_operation":
        return True
    if policy == "at_end":
        return index == count - 1
    for other in operations:
        if op["id"] in other["depends_on"]:
            return True
    return index == count - 1


def _condition_allows(
    op: dict[str, Any], status_by_id: dict[str, str]
) -> tuple[bool, str]:
    condition = op["object"].get("condition")
    if not isinstance(condition, dict):
        return True, ""
    status_condition = condition.get("operation_status")
    if not isinstance(status_condition, dict):
        return True, ""
    source_id = status_condition.get("id")
    allowed = status_condition.get("is")
    if not isinstance(source_id, str) or not isinstance(allowed, list):
        raise PlanExecutorError(
            f"operation '{op['id']}' has malformed operation_status condition"
        )
    actual = status_by_id.get(source_id)
    if actual is None:
        raise PlanExecutorError(
            f"operation '{op['id']}' condition references unavailable operation '{source_id}'"
        )
    if actual in allowed:
        return True, ""
    return False, f"condition not met: operation '{source_id}' status was '{actual}'"


def _build_nested_request(
    root: dict[str, Any],
    op: dict[str, Any],
    completed: dict[str, dict[str, Any]],
    compile_flag: bool,
    validate_flag: bool,
) -> dict[str, Any]:
    request: dict[str, Any] = {
        "protocol_version": PROTOCOL_VERSION,
        "action": op["action"],
    }
    parent_id = root.get("request_id")
    if isinstance(parent_id, str) and parent_id:
        request["request_id"] = f"{parent_id}:{op['id']}"
    for field in ("project", "target", "mode", "expected_revision", "idempotency_key"):
        if field in root:
            request[field] = copy.deepcopy(root[field])
    for field in ("target", "mode", "expected_revision"):
        if field in op["object"]:
            request[field] = copy.deepcopy(op["object"][field])
    specification = copy.deepcopy(op["object"].get("specification") or {})
    request["specification"] = resolve_refs(specification, completed)
    options = copy.deepcopy(root.get("options") or {})
    if not isinstance(options, dict):
        options = {}
    options["compile"] = compile_flag
    options["validate"] = validate_flag
    options["timeout_ms"] = 0
    request["options"] = options
    return request


def _op_result(
    op: dict[str, Any],
    status: str,
    summary: str,
    skipped: str = "",
) -> dict[str, Any]:
    out = {
        "id": op["id"],
        "action": op["action"],
        "status": status,
        "summary": summary,
    }
    if skipped:
        out["skipped_reason"] = skipped
    return out


# Module-level singleton used by offline tests, mirroring FUeremcpPlanExecutor::Get-style state.
DEFAULT = PlanExecutor()
