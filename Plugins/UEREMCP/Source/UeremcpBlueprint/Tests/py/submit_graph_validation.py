"""Python mirror of FUeremcpBlueprintGraphWriter::ValidateSubmittedGraphForReplace."""

from __future__ import annotations

from typing import Any

from graph_json_to_dsl import GraphJsonToDslError, resolve_write_dsl


BLUEPRINT_GRAPH_TYPE_PREFIX = "Blueprint"


class SubmitGraphValidationError(Exception):
    def __init__(self, message: str, capability_notes: list[str] | None = None) -> None:
        super().__init__(message)
        self.capability_notes = list(capability_notes or [])


def validate_submitted_graph_for_replace(
    graph: dict[str, Any],
    expected_asset_path: str,
    expected_graph_id: str,
    *,
    require_write_dsl: bool = True,
) -> None:
    """Raise SubmitGraphValidationError when the submitted graph cannot be written."""
    notes: list[str] = []

    for field in ("asset_path", "graph_id", "graph_type", "schema_version"):
        value = graph.get(field)
        if not isinstance(value, str) or not value.strip():
            raise SubmitGraphValidationError(f"submitted graph missing required field '{field}'")

    fidelity = graph.get("fidelity")
    if not isinstance(fidelity, dict):
        raise SubmitGraphValidationError("submitted graph missing required object 'fidelity'")
    if "round_trip_supported" not in fidelity or not isinstance(fidelity["round_trip_supported"], bool):
        raise SubmitGraphValidationError("submitted graph.fidelity.round_trip_supported is required")

    graph_type = graph["graph_type"]
    if not isinstance(graph_type, str) or not graph_type.startswith(BLUEPRINT_GRAPH_TYPE_PREFIX):
        notes.append("submit_graph.unsupported_graph_type")
        raise SubmitGraphValidationError(
            f"submit_graph replace supports Blueprint graph_type values only; got '{graph_type}'",
            notes,
        )

    asset_path = graph["asset_path"]
    if expected_asset_path and asset_path != expected_asset_path:
        notes.append("submit_graph.asset_path_mismatch")
        raise SubmitGraphValidationError(
            f"submitted graph asset_path '{asset_path}' does not match target '{expected_asset_path}'",
            notes,
        )

    graph_id = graph["graph_id"]
    if expected_graph_id and graph_id != expected_graph_id:
        notes.append("submit_graph.graph_id_mismatch")
        raise SubmitGraphValidationError(
            f"submitted graph graph_id '{graph_id}' does not match target '{expected_graph_id}'",
            notes,
        )

    nodes = graph.get("nodes")
    if not isinstance(nodes, list):
        raise SubmitGraphValidationError("submitted graph missing required array 'nodes'")

    if require_write_dsl:
        try:
            dsl, _lossy = resolve_write_dsl(graph)
        except GraphJsonToDslError as exc:
            notes.append("submit_graph.dsl_required")
            raise SubmitGraphValidationError(str(exc), notes) from exc

        if not isinstance(dsl, str) or not dsl.strip():
            notes.append("submit_graph.dsl_required")
            raise SubmitGraphValidationError("resolved DSL is empty", notes)
