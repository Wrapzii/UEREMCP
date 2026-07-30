"""UEREMCP protocol reference implementations — runnable outside the Unreal editor.

Mirrors Plugins/UEREMCP/Source/UeremcpProtocol C++ behaviour for unit tests.
Owner: WS-05.
"""

from .content_hash import canonicalise_for_hash, content_hash
from .dependency_order import topological_sort
from .envelope import parse_request, serialize_response, PROTOCOL_VERSION
from .ref_resolve import resolve_refs

__all__ = [
    "PROTOCOL_VERSION",
    "canonicalise_for_hash",
    "content_hash",
    "parse_request",
    "serialize_response",
    "resolve_refs",
    "topological_sort",
]
