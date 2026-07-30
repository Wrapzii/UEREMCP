"""Deterministic content hashing — see Docs/CONTENT_HASH.md."""

from __future__ import annotations

import hashlib
import json
import re
from typing import Any


_GUID_KEY = re.compile(r"(^guid$|_guid$|^nodeguid$|^node_guid$|^pin_guid$)", re.I)

_IGNORED = frozenset(
    {
        "position",
        "bounds",
        "content_hash",
        "revision",
        "retrieved_at",
        "node_id",
        "pin_id",
        "comment_id",
    }
)


def _is_ignored_key(key: str) -> bool:
    return key in _IGNORED or bool(_GUID_KEY.match(key))


def _stable_node_key(node: dict) -> str:
    sem = node.get("semantic_id")
    if isinstance(sem, str) and sem:
        return f"sem:{sem}"
    return (
        f"cls:{node.get('node_class', '')}"
        f"|type:{node.get('semantic_type', '')}"
        f"|title:{node.get('title', '')}"
    )


def _pin_stable_name(pin: dict) -> str:
    return f"{pin.get('direction', '')}:{pin.get('name', '')}"


def _sort_json(value: Any) -> Any:
    if isinstance(value, dict):
        return {k: _sort_json(value[k]) for k in sorted(value)}
    if isinstance(value, list):
        return [_sort_json(v) for v in value]
    return value


def _canonicalise_pin(pin: dict) -> dict:
    out: dict[str, Any] = {}
    for key in sorted(pin):
        if _is_ignored_key(key):
            continue
        if key == "links":
            # Pin-level links duplicate top-level `links[]` and embed retrieval-local
            # node_id/pin_id. Omit them; connections are hashed via the graph edge list.
            continue
        out[key] = _canonicalise_value(pin[key])
    return out


def _canonicalise_node(node: dict) -> dict:
    out: dict[str, Any] = {"_stable_key": _stable_node_key(node)}
    for key in sorted(node):
        if _is_ignored_key(key) or key == "semantic_id":
            continue
        if key in ("input_pins", "output_pins"):
            pins = [
                _canonicalise_pin(p)
                for p in (node.get(key) or [])
                if isinstance(p, dict)
            ]
            pins.sort(
                key=lambda p: f"{p.get('direction', '')}|{p.get('name', '')}"
            )
            out[key] = pins
            continue
        out[key] = _canonicalise_value(node[key])
    return out


def _canonicalise_graph(obj: dict) -> dict:
    nodes = [n for n in (obj.get("nodes") or []) if isinstance(n, dict)]
    node_id_to_stable = {}
    node_pin_id_to_name: dict[str, dict[str, str]] = {}
    for node in nodes:
        nid = node.get("node_id", "")
        stable = _stable_node_key(node)
        if nid:
            node_id_to_stable[nid] = stable
        pin_map: dict[str, str] = {}
        for field in ("input_pins", "output_pins"):
            for pin in node.get(field) or []:
                if isinstance(pin, dict) and pin.get("pin_id"):
                    pin_map[pin["pin_id"]] = _pin_stable_name(pin)
        if nid:
            node_pin_id_to_name[nid] = pin_map

    out: dict[str, Any] = {}
    for key in sorted(obj):
        if _is_ignored_key(key):
            continue
        if key == "nodes":
            canon_nodes = [_canonicalise_node(n) for n in nodes]
            canon_nodes.sort(key=lambda n: n["_stable_key"])
            out["nodes"] = canon_nodes
            continue
        if key == "links":
            canon_links = []
            for link in obj.get("links") or []:
                if not isinstance(link, dict):
                    continue
                fn = link.get("from_node", "")
                tn = link.get("to_node", "")
                fp = link.get("from_pin", "")
                tp = link.get("to_pin", "")
                cl = {
                    "from_node": node_id_to_stable.get(fn, fn),
                    "from_pin": node_pin_id_to_name.get(fn, {}).get(fp, fp),
                    "to_node": node_id_to_stable.get(tn, tn),
                    "to_pin": node_pin_id_to_name.get(tn, {}).get(tp, tp),
                }
                if link.get("kind"):
                    cl["kind"] = link["kind"]
                canon_links.append(cl)
            canon_links.sort(
                key=lambda L: (
                    f"{L.get('from_node','')}>{L.get('from_pin','')}>"
                    f"{L.get('to_node','')}>{L.get('to_pin','')}>{L.get('kind','')}"
                )
            )
            out["links"] = canon_links
            continue
        out[key] = _canonicalise_value(obj[key])
    return out


def _canonicalise_value(value: Any) -> Any:
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, list):
        return [_canonicalise_value(v) for v in value]
    if isinstance(value, dict):
        if "nodes" in value or "graph_type" in value:
            return _canonicalise_graph(value)
        return {
            k: _canonicalise_value(value[k])
            for k in sorted(value)
            if not _is_ignored_key(k)
        }
    return value


def canonicalise_for_hash(value: Any) -> Any:
    return _sort_json(_canonicalise_value(value))


def content_hash(value: Any) -> str:
    """Return sha256:<hex> over canonical JSON bytes."""
    if isinstance(value, str):
        value = json.loads(value)
    canon = canonicalise_for_hash(value)
    payload = json.dumps(canon, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    digest = hashlib.sha256(payload.encode("utf-8")).hexdigest()
    return f"sha256:{digest}"
