"""Python mirror of FUeremcpBlueprintGraphWriter JSON→DSL translator (offline goldens).

Keep in sync with UeremcpBlueprintGraphWriter.cpp::TryTranslateGraphJsonToDsl.
"""

from __future__ import annotations

from typing import Any


LOSSY_TRANSLATOR_NOTES = (
    "graph_json_to_dsl_minimal_translator",
    "exotic_nodes_require_extensions_blueprint_dsl",
)


class GraphJsonToDslError(Exception):
    pass


def _build_node_map(graph: dict[str, Any]) -> dict[str, dict[str, Any]]:
    out: dict[str, dict[str, Any]] = {}
    for node in graph.get("nodes") or []:
        if isinstance(node, dict):
            node_id = node.get("node_id")
            if isinstance(node_id, str):
                out[node_id] = node
    return out


def _get_then_target(node: dict[str, Any]) -> str | None:
    for pin in node.get("output_pins") or []:
        if not isinstance(pin, dict):
            continue
        if pin.get("name") != "then":
            continue
        links = pin.get("links") or []
        if not links:
            return None
        link = links[0]
        if isinstance(link, dict):
            target = link.get("node_id")
            return target if isinstance(target, str) else None
    return None


def _is_event_entry_node(node: dict[str, Any]) -> bool:
    semantic_type = node.get("semantic_type")
    if isinstance(semantic_type, str):
        if semantic_type.startswith("event:") or semantic_type.startswith("custom_event:"):
            return True
    properties = node.get("properties")
    if isinstance(properties, dict):
        type_id = properties.get("type_id")
        if isinstance(type_id, str):
            return type_id.startswith("Event|") or type_id.startswith("AddEvent|Custom|")
    return False


def _resolve_event_name(node: dict[str, Any]) -> str | None:
    semantic_type = node.get("semantic_type")
    if isinstance(semantic_type, str):
        if semantic_type.startswith("event:"):
            name = semantic_type[6:]
            return name or None
        if semantic_type.startswith("custom_event:"):
            name = semantic_type[13:]
            return name or None
    properties = node.get("properties")
    if isinstance(properties, dict):
        type_id = properties.get("type_id")
        if type_id == "Event|ReceiveBeginPlay":
            return "EventBeginPlay"
        if isinstance(type_id, str) and type_id.startswith("AddEvent|Custom|"):
            name = type_id[16:]
            return name or None
    return None


def _format_default_value(value: Any) -> str:
    if isinstance(value, str):
        escaped = value.replace('"', '\\"')
        return f'"{escaped}"'
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return str(value)
    return ""


def _build_call_inner(node: dict[str, Any]) -> str:
    properties = node.get("properties")
    if not isinstance(properties, dict):
        raise GraphJsonToDslError("call node missing properties.type_id")
    type_id = properties.get("type_id")
    if not isinstance(type_id, str) or not type_id:
        raise GraphJsonToDslError("call node missing properties.type_id")

    args = [type_id]
    defaults = node.get("defaults")
    if isinstance(defaults, dict):
        for key in sorted(defaults):
            formatted = _format_default_value(defaults[key])
            if formatted:
                args.append(f":{key} {formatted}")
    return " ".join(args)


def _emit_linear_exec_chain(start_node_id: str, node_by_id: dict[str, dict[str, Any]], indent: int) -> str:
    current_id: str | None = start_node_id
    while current_id:
        node = node_by_id.get(current_id)
        if node is None:
            raise GraphJsonToDslError(f"exec chain references unknown node '{current_id}'")

        if _is_event_entry_node(node):
            current_id = _get_then_target(node)
            continue

        inner = _build_call_inner(node)
        pad = " " * indent
        next_id = _get_then_target(node)
        if next_id:
            nested = _emit_linear_exec_chain(next_id, node_by_id, indent + 2)
            return f"{pad}({inner}\n{nested})"
        return f"{pad}({inner})"
    return ""


def translate_graph_json_to_dsl(graph: dict[str, Any]) -> tuple[str, list[str]]:
    """Return (dsl, lossy_notes). Raises GraphJsonToDslError on failure."""
    entry_points = graph.get("entry_points")
    if not isinstance(entry_points, list) or not entry_points:
        raise GraphJsonToDslError(
            "graph JSON translation requires entry_points and extensions.blueprint.dsl is absent"
        )

    node_by_id = _build_node_map(graph)
    event_blocks: list[str] = []

    for entry_value in entry_points:
        if not isinstance(entry_value, str):
            raise GraphJsonToDslError("entry_points must be node_id strings")
        entry_node = node_by_id.get(entry_value)
        if entry_node is None:
            raise GraphJsonToDslError(f"entry point '{entry_value}' not found in nodes")

        event_name = _resolve_event_name(entry_node)
        if not event_name:
            raise GraphJsonToDslError(
                f"entry point '{entry_value}' is not a supported event/custom_event node"
            )

        body_start = _get_then_target(entry_node)
        body_dsl = _emit_linear_exec_chain(body_start, node_by_id, 2) if body_start else ""

        if body_dsl:
            event_blocks.append(f"(event {event_name}\n{body_dsl})")
        else:
            event_blocks.append(f"(event {event_name})")

    return "\n".join(event_blocks), list(LOSSY_TRANSLATOR_NOTES)


def resolve_write_dsl(graph: dict[str, Any]) -> tuple[str, list[str]]:
    """Mirror ResolveWriteDsl: prefer extensions.blueprint.dsl, else translate."""
    extensions = graph.get("extensions")
    if isinstance(extensions, dict):
        blueprint_ext = extensions.get("blueprint")
        if isinstance(blueprint_ext, dict):
            dsl = blueprint_ext.get("dsl")
            if isinstance(dsl, str) and dsl.strip():
                return dsl, []
    return translate_graph_json_to_dsl(graph)
