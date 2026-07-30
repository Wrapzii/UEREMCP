"""UEREMCP protocol reference implementations — runnable outside the Unreal editor.

Mirrors Plugins/UEREMCP/Source/UeremcpProtocol C++ behaviour for unit tests.
Owner: WS-05.
"""

from .content_hash import canonicalise_for_hash, content_hash
from .dependency_order import topological_sort
from .envelope import parse_request, serialize_response, PROTOCOL_VERSION
from .job import (
    DEFAULT_TIMEOUT_MS,
    make_job_timeout_response,
    should_dispatch_inline,
)
from .ref_resolve import resolve_refs

__all__ = [
    "PROTOCOL_VERSION",
    "DEFAULT_TIMEOUT_MS",
    "canonicalise_for_hash",
    "content_hash",
    "make_job_timeout_response",
    "parse_request",
    "serialize_response",
    "resolve_refs",
    "should_dispatch_inline",
    "topological_sort",
]
