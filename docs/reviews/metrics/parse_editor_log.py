"""Parse Unreal editor logs for POC-B CreateNiagaraEffect timing markers.

Server-side interval from dispatch → final synchronous content-validation is a
*lower bound*, never wall_clock_seconds.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable


# UE log timestamp: [2026.07.30-11.14.53:492][…]
_TS = re.compile(
    r"\[(?P<y>\d{4})\.(?P<mo>\d{2})\.(?P<d>\d{2})-"
    r"(?P<h>\d{2})\.(?P<mi>\d{2})\.(?P<s>\d{2}):(?P<ms>\d{3})\]"
)

# Markers recorded by WS-11 for the successful B1 run (orch d07f8f1).
# Keep patterns loose enough to survive log wording drift; tests pin fixtures.
DEFAULT_DISPATCH_PATTERNS = (
    re.compile(r"CreateNiagaraEffect", re.I),
    re.compile(r"UeremcpNiagara.*[Dd]ispatch", re.I),
    re.compile(r"LogUeremcpNiagara:.*[Cc]reate", re.I),
)
DEFAULT_COMPLETION_PATTERNS = (
    re.compile(r"content[-_ ]validation", re.I),
    re.compile(r"reread_after_write", re.I),
    re.compile(r"B1_single_request_complete", re.I),
    re.compile(r"CreateNiagaraEffect.*complet", re.I),
)


@dataclass(frozen=True)
class LogHit:
    line_no: int  # 1-based
    epoch_s: float
    line: str


def parse_ue_timestamp(line: str) -> float | None:
    m = _TS.search(line)
    if not m:
        return None
    dt = datetime(
        int(m["y"]),
        int(m["mo"]),
        int(m["d"]),
        int(m["h"]),
        int(m["mi"]),
        int(m["s"]),
        int(m["ms"]) * 1000,
    )
    return dt.timestamp()


def iter_hits(lines: Iterable[str], patterns: tuple[re.Pattern[str], ...]) -> list[LogHit]:
    hits: list[LogHit] = []
    for i, line in enumerate(lines, start=1):
        if not any(p.search(line) for p in patterns):
            continue
        epoch = parse_ue_timestamp(line)
        if epoch is None:
            continue
        hits.append(LogHit(line_no=i, epoch_s=epoch, line=line.rstrip("\n")))
    return hits


def measure_server_side_interval(
    log_text: str,
    *,
    dispatch_patterns: tuple[re.Pattern[str], ...] = DEFAULT_DISPATCH_PATTERNS,
    completion_patterns: tuple[re.Pattern[str], ...] = DEFAULT_COMPLETION_PATTERNS,
) -> dict:
    """Return measured server-side lower bound or an honest unavailable result."""
    lines = log_text.splitlines()
    dispatch_hits = iter_hits(lines, dispatch_patterns)
    completion_hits = iter_hits(lines, completion_patterns)

    if not dispatch_hits:
        return {
            "status": "unavailable",
            "value_seconds": None,
            "reason": "no dispatch marker matched",
            "dispatch": None,
            "completion": None,
        }
    if not completion_hits:
        return {
            "status": "unavailable",
            "value_seconds": None,
            "reason": "no completion marker matched",
            "dispatch": _hit_dict(dispatch_hits[0]),
            "completion": None,
        }

    # Pair: earliest dispatch, then first completion at/after that timestamp.
    start = dispatch_hits[0]
    end_candidates = [h for h in completion_hits if h.epoch_s >= start.epoch_s]
    if not end_candidates:
        return {
            "status": "unavailable",
            "value_seconds": None,
            "reason": "no completion at or after dispatch",
            "dispatch": _hit_dict(start),
            "completion": None,
        }
    end = end_candidates[0]
    return {
        "status": "measured",
        "value_seconds": round(end.epoch_s - start.epoch_s, 6),
        "reason": "server-side lower bound from editor log markers; not wall_clock_seconds",
        "dispatch": _hit_dict(start),
        "completion": _hit_dict(end),
    }


def measure_server_side_interval_file(path: str | Path, **kwargs) -> dict:
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    result = measure_server_side_interval(text, **kwargs)
    result["log_path"] = str(path)
    return result


def _hit_dict(hit: LogHit) -> dict:
    return {"line_no": hit.line_no, "epoch_s": hit.epoch_s, "line": hit.line}
