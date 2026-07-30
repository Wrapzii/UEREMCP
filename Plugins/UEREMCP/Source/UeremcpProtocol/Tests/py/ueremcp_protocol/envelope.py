"""Request/response envelope — mirrors FUeremcpEnvelope against frozen schemas."""

from __future__ import annotations

import json
import re
from typing import Any


PROTOCOL_VERSION = "1.0"

_ACTION_RE = re.compile(r"^[a-z][a-z0-9_]*$")
_VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+$")

MODES = frozenset(
    {
        "create",
        "create_or_update",
        "replace",
        "patch",
        "rebuild_from_specification",
        "repair",
        "delete",
    }
)
STATUSES = frozenset(
    {
        "created_and_validated",
        "modified_and_validated",
        "created_with_warnings",
        "no_change_required",
        "failed_validation",
        "rolled_back",
        "partially_completed",
        "rejected",
        "error",
    }
)
RESPONSE_DETAILS = frozenset({"minimal", "summary", "diagnostic", "complete"})
CONFLICT_POLICIES = frozenset(
    {"reject", "return_conflict", "merge", "replace", "force"}
)

_TOP_ALLOWED = frozenset(
    {
        "protocol_version",
        "request_id",
        "action",
        "project",
        "target",
        "mode",
        "specification",
        "options",
        "expected_revision",
        "idempotency_key",
    }
)
_TARGET_ALLOWED = frozenset(
    {"asset_path", "object_path", "graph_id", "actor_label"}
)
_OPTIONS_ALLOWED = frozenset(
    {
        "dry_run",
        "atomic",
        "rollback_on_failure",
        "compile",
        "validate",
        "save",
        "response_detail",
        "timeout_ms",
        "on_revision_conflict",
        "continue_on_error",
        "allow_destructive",
    }
)


class EnvelopeError(ValueError):
    pass


def is_protocol_compatible(other: str) -> bool:
    if not _VERSION_RE.match(other):
        return False
    return other.split(".", 1)[0] == PROTOCOL_VERSION.split(".", 1)[0]


def parse_request(json_text: str) -> dict[str, Any]:
    try:
        root = json.loads(json_text)
    except json.JSONDecodeError as exc:
        raise EnvelopeError("request is not a JSON object") from exc
    if not isinstance(root, dict):
        raise EnvelopeError("request is not a JSON object")

    unknown = set(root) - _TOP_ALLOWED
    if unknown:
        raise EnvelopeError(f"unknown top-level field '{sorted(unknown)[0]}'")

    if "protocol_version" not in root or "action" not in root:
        raise EnvelopeError("missing required field protocol_version or action")

    version = root["protocol_version"]
    if not isinstance(version, str) or not _VERSION_RE.match(version):
        raise EnvelopeError(
            f"protocol_version '{version}' does not match MAJOR.MINOR"
        )

    action = root["action"]
    if not isinstance(action, str) or not _ACTION_RE.match(action):
        raise EnvelopeError(f"action '{action}' does not match ^[a-z][a-z0-9_]*$")

    out: dict[str, Any] = {
        "protocol_version": version,
        "action": action,
        "request_id": "",
        "mode": "create_or_update",
        "project_path": "",
        "engine_version": "",
        "target_asset_path": "",
        "target_object_path": "",
        "target_graph_id": "",
        "target_actor_label": "",
        "specification": None,
        "dry_run": False,
        "atomic": True,
        "rollback_on_failure": True,
        "compile": True,
        "validate": True,
        "save": True,
        "response_detail": "summary",
        "timeout_ms": 0,
        "on_revision_conflict": "reject",
        "continue_on_error": False,
        "allow_destructive": False,
        "expected_revision": None,
        "has_expected_revision": False,
        "idempotency_key": "",
    }

    if "request_id" in root and root["request_id"] is not None:
        rid = root["request_id"]
        if not isinstance(rid, str) or not (1 <= len(rid) <= 128):
            raise EnvelopeError("request_id must be 1..128 characters")
        out["request_id"] = rid

    project = root.get("project")
    if isinstance(project, dict):
        out["project_path"] = project.get("path") or ""
        out["engine_version"] = project.get("engine_version") or ""

    target = root.get("target")
    if isinstance(target, dict):
        unknown_t = set(target) - _TARGET_ALLOWED
        if unknown_t:
            raise EnvelopeError(f"unknown target field '{sorted(unknown_t)[0]}'")
        out["target_asset_path"] = target.get("asset_path") or ""
        out["target_object_path"] = target.get("object_path") or ""
        out["target_graph_id"] = target.get("graph_id") or ""
        out["target_actor_label"] = target.get("actor_label") or ""

    if "mode" in root and root["mode"] is not None:
        if root["mode"] not in MODES:
            raise EnvelopeError(f"invalid mode '{root['mode']}'")
        out["mode"] = root["mode"]

    if "specification" in root and root["specification"] is not None:
        if not isinstance(root["specification"], dict):
            raise EnvelopeError("specification must be an object")
        out["specification"] = root["specification"]

    options = root.get("options")
    if isinstance(options, dict):
        unknown_o = set(options) - _OPTIONS_ALLOWED
        if unknown_o:
            raise EnvelopeError(f"unknown options field '{sorted(unknown_o)[0]}'")
        for key, default in (
            ("dry_run", False),
            ("atomic", True),
            ("rollback_on_failure", True),
            ("compile", True),
            ("validate", True),
            ("save", True),
            ("continue_on_error", False),
            ("allow_destructive", False),
        ):
            out[key] = bool(options[key]) if key in options else default
        if "response_detail" in options:
            if options["response_detail"] not in RESPONSE_DETAILS:
                raise EnvelopeError(
                    f"invalid response_detail '{options['response_detail']}'"
                )
            out["response_detail"] = options["response_detail"]
        if "timeout_ms" in options:
            timeout = options["timeout_ms"]
            if not isinstance(timeout, int) or timeout < 0:
                raise EnvelopeError("timeout_ms must be >= 0")
            out["timeout_ms"] = timeout
        if "on_revision_conflict" in options:
            if options["on_revision_conflict"] not in CONFLICT_POLICIES:
                raise EnvelopeError(
                    f"invalid on_revision_conflict '{options['on_revision_conflict']}'"
                )
            out["on_revision_conflict"] = options["on_revision_conflict"]

    if "expected_revision" in root:
        if root["expected_revision"] is None:
            out["has_expected_revision"] = False
            out["expected_revision"] = None
        else:
            out["has_expected_revision"] = True
            out["expected_revision"] = root["expected_revision"]

    if "idempotency_key" in root and root["idempotency_key"] is not None:
        key = root["idempotency_key"]
        if not isinstance(key, str) or len(key) > 128:
            raise EnvelopeError("idempotency_key maxLength is 128")
        out["idempotency_key"] = key

    return out


def serialize_response(response: dict[str, Any]) -> str:
    from .job import validate_job

    status = response.get("status", "")
    if status not in STATUSES:
        raise EnvelopeError(f"invalid status '{status}'")
    summary = response.get("summary", "")
    if not summary:
        raise EnvelopeError("summary is required")

    root: dict[str, Any] = {
        "protocol_version": response.get("protocol_version") or PROTOCOL_VERSION,
        "status": status,
        "summary": summary,
        "metrics": {
            "mcp_round_trips": int(response.get("metrics", {}).get("mcp_round_trips", 0)),
            "internal_operations": int(
                response.get("metrics", {}).get("internal_operations", 0)
            ),
        },
    }
    if response.get("request_id"):
        root["request_id"] = response["request_id"]

    understood: dict[str, Any] = {}
    if response.get("understood_action"):
        understood["action"] = response["understood_action"]
    if response.get("understood_target"):
        understood["resolved_target"] = response["understood_target"]
    if understood:
        root["understood"] = understood

    if response.get("primary_asset"):
        root["result"] = {"primary_asset": response["primary_asset"]}
    if response.get("revision"):
        root["revision"] = response["revision"]

    job = response.get("job")
    if job is not None:
        if not isinstance(job, dict):
            raise EnvelopeError("job must be an object")
        try:
            validate_job(job)
        except Exception as exc:  # JobError
            raise EnvelopeError(str(exc)) from exc
        state = job["state"]
        if state in ("running", "queued") and status not in (
            "partially_completed",
            "error",
        ):
            raise EnvelopeError(
                "in-flight job handle requires status partially_completed (or error)"
            )
        job_out: dict[str, Any] = {
            "job_id": job["job_id"],
            "state": state,
            "poll_action": job.get("poll_action") or "get_job_result",
        }
        if "progress" in job:
            job_out["progress"] = job["progress"]
        if job.get("progress_message"):
            job_out["progress_message"] = job["progress_message"]
        if "cancellable" in job:
            job_out["cancellable"] = bool(job["cancellable"])
        root["job"] = job_out

    if response.get("capability_notes"):
        root["capability_notes"] = list(response["capability_notes"])

    metrics = response.get("metrics") or {}
    if metrics.get("timing_ms"):
        root["metrics"]["timing_ms"] = metrics["timing_ms"]
    if metrics.get("assets_affected"):
        root["metrics"]["assets_affected"] = metrics["assets_affected"]
    if metrics.get("replayed"):
        root["metrics"]["replayed"] = True

    return json.dumps(root, ensure_ascii=False)


def make_rejection(request_id: str, reason: str) -> str:
    return serialize_response(
        {
            "request_id": request_id,
            "status": "rejected",
            "summary": reason,
            "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
        }
    )
