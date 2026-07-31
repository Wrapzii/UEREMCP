"""Mountain–river–rain offline acceptance harness (WS-11).

Evaluates evidence produced by an isolated editor process against
MOUNTAIN_RIVER_RAIN_ACCEPTANCE.md. Never connects to a live builder editor.

Usage:
  python tests/visual/mountain_river_rain_harness.py <evidence_dir>
  python tests/visual/mountain_river_rain_harness.py --self-test
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path
from typing import Any

REQUIRED_PNGS = (
    "player_start.png",
    "river_upstream.png",
    "river_downstream.png",
    "valley_cross_section.png",
    "rain_camera_a.png",
    "rain_camera_b.png",
    "overview.png",
)

REQUIRED_JSON = ("structure.json", "reload.json", "performance.json")


def _png_header(path: Path) -> tuple[int, int] | None:
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    width, height = struct.unpack(">II", data[16:24])
    return int(width), int(height)


def _gate(name: str, status: str, detail: str) -> dict[str, str]:
    return {"gate": name, "status": status, "detail": detail}


def evaluate_structure(structure: dict[str, Any]) -> list[dict[str, str]]:
    gates: list[dict[str, str]] = []
    elev = structure.get("terrain_elevation_range_uu")
    if not isinstance(elev, (int, float)):
        gates.append(_gate("landscape_valley", "BLOCKED", "missing terrain_elevation_range_uu"))
    elif elev < 1000:
        gates.append(_gate("landscape_valley", "FAIL", f"elevation range {elev} < 1000"))
    else:
        shoulders = structure.get("valley_shoulder_drop_uu")
        if isinstance(shoulders, (int, float)) and shoulders >= 300:
            gates.append(_gate("landscape_valley", "PASS", f"range={elev} drop={shoulders}"))
        elif shoulders is None:
            gates.append(_gate("landscape_valley", "BLOCKED", "missing valley_shoulder_drop_uu"))
        else:
            gates.append(_gate("landscape_valley", "FAIL", f"shoulder drop {shoulders} < 300"))

    samples = structure.get("river_centerline_samples") or structure.get("river_corridor_samples")
    if not isinstance(samples, list) or len(samples) < 2:
        gates.append(_gate("river_continuity", "BLOCKED", "missing ordered river samples"))
    else:
        gaps = structure.get("river_dry_gap_count", 0)
        if gaps:
            gates.append(_gate("river_continuity", "FAIL", f"dry_gap_count={gaps}"))
        else:
            gates.append(_gate("river_continuity", "PASS", f"samples={len(samples)}"))

    exclusions = structure.get("foliage_channel_intersections")
    bank_sections = structure.get("trees_both_banks_sections")
    if exclusions is None or bank_sections is None:
        gates.append(_gate("foliage_exclusion", "BLOCKED", "missing foliage exclusion metrics"))
    elif exclusions != 0:
        gates.append(_gate("foliage_exclusion", "FAIL", f"intersections={exclusions}"))
    elif not isinstance(bank_sections, int) or bank_sections < 3:
        gates.append(_gate("foliage_exclusion", "FAIL", f"both-bank sections={bank_sections}"))
    else:
        gates.append(_gate("foliage_exclusion", "PASS", f"sections={bank_sections}"))

    rain = structure.get("rain") or {}
    follow_err = rain.get("camera_follow_world_error_pct")
    local_delta = rain.get("local_offset_delta_uu")
    if follow_err is None or local_delta is None:
        gates.append(_gate("camera_follow_rain", "BLOCKED", "missing rain follow metrics"))
    elif follow_err > 10.0 or local_delta > 100.0:
        gates.append(
            _gate(
                "camera_follow_rain",
                "FAIL",
                f"follow_err_pct={follow_err} local_delta={local_delta}",
            )
        )
    else:
        gates.append(
            _gate(
                "camera_follow_rain",
                "PASS",
                f"follow_err_pct={follow_err} local_delta={local_delta}",
            )
        )

    compile_ok = structure.get("compile_ok")
    if compile_ok is True:
        gates.append(_gate("compile_renderer", "PASS", "compile_ok=true"))
    elif compile_ok is False:
        gates.append(_gate("compile_renderer", "FAIL", "compile_ok=false"))
    else:
        gates.append(_gate("compile_renderer", "BLOCKED", "missing compile_ok"))

    return gates


def evaluate_reload(reload: dict[str, Any]) -> dict[str, str]:
    if not reload.get("before_hash") or not reload.get("after_hash"):
        return _gate("reload", "BLOCKED", "missing before/after package hashes")
    if reload["before_hash"] != reload["after_hash"]:
        return _gate("reload", "FAIL", "package hash drifted across restart")
    if reload.get("load_errors"):
        return _gate("reload", "FAIL", f"load_errors={reload['load_errors']}")
    return _gate("reload", "PASS", "hashes match; no load errors")


def evaluate_performance(perf: dict[str, Any]) -> dict[str, str]:
    actors = perf.get("loaded_actors")
    trees = perf.get("tree_instance_count")
    rain_particles = perf.get("peak_rain_particles")
    p95 = perf.get("p95_frame_ms")
    missing = [k for k, v in (
        ("loaded_actors", actors),
        ("tree_instance_count", trees),
        ("peak_rain_particles", rain_particles),
        ("p95_frame_ms", p95),
    ) if v is None]
    if missing:
        return _gate("performance", "BLOCKED", f"missing {missing}")
    fails = []
    if actors > 2500:
        fails.append(f"actors={actors}")
    if trees > 100000:
        fails.append(f"trees={trees}")
    if rain_particles > 150000:
        fails.append(f"rain={rain_particles}")
    if p95 > 33.3:
        fails.append(f"p95={p95}")
    if fails:
        return _gate("performance", "FAIL", "; ".join(fails))
    return _gate("performance", "PASS", f"actors={actors} trees={trees} p95={p95}")


def evaluate_pngs(evidence_dir: Path) -> list[dict[str, str]]:
    gates: list[dict[str, str]] = []
    for name in REQUIRED_PNGS:
        path = evidence_dir / name
        if not path.exists():
            gates.append(_gate(f"png:{name}", "BLOCKED", "missing file"))
            continue
        header = _png_header(path)
        if header is None:
            gates.append(_gate(f"png:{name}", "FAIL", "not a readable PNG"))
            continue
        w, h = header
        if (w, h) != (1920, 1080):
            gates.append(_gate(f"png:{name}", "FAIL", f"dims={w}x{h} expected 1920x1080"))
        elif path.stat().st_size < 1024:
            gates.append(_gate(f"png:{name}", "FAIL", "file too small"))
        else:
            gates.append(_gate(f"png:{name}", "PASS", f"dims={w}x{h} bytes={path.stat().st_size}"))
    return gates


def evaluate_evidence_dir(evidence_dir: Path) -> dict[str, Any]:
    gates: list[dict[str, str]] = []
    payload: dict[str, Any] = {}
    for name in REQUIRED_JSON:
        path = evidence_dir / name
        if not path.exists():
            gates.append(_gate(name, "BLOCKED", "missing report"))
            continue
        try:
            payload[name] = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            gates.append(_gate(name, "FAIL", f"unreadable: {exc}"))

    if "structure.json" in payload:
        gates.extend(evaluate_structure(payload["structure.json"]))
    if "reload.json" in payload:
        gates.append(evaluate_reload(payload["reload.json"]))
    if "performance.json" in payload:
        gates.append(evaluate_performance(payload["performance.json"]))
    gates.extend(evaluate_pngs(evidence_dir))

    statuses = {g["status"] for g in gates}
    if "FAIL" in statuses:
        overall = "FAIL"
    elif "BLOCKED" in statuses:
        overall = "BLOCKED"
    else:
        overall = "PASS"

    return {
        "protocol": "mountain_river_rain_acceptance/v1",
        "evidence_dir": str(evidence_dir),
        "overall": overall,
        "gates": gates,
        "visual_policy": "screenshots supplemental; structural gates authoritative",
    }


def _self_test() -> int:
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "structure.json").write_text(
            json.dumps(
                {
                    "terrain_elevation_range_uu": 2400,
                    "valley_shoulder_drop_uu": 450,
                    "river_centerline_samples": [[0, 0, 0], [1000, 0, 0], [2000, 0, 0]],
                    "river_dry_gap_count": 0,
                    "foliage_channel_intersections": 0,
                    "trees_both_banks_sections": 3,
                    "rain": {"camera_follow_world_error_pct": 4.0, "local_offset_delta_uu": 20.0},
                    "compile_ok": True,
                }
            ),
            encoding="utf-8",
        )
        (root / "reload.json").write_text(
            json.dumps({"before_hash": "abc", "after_hash": "abc", "load_errors": []}),
            encoding="utf-8",
        )
        (root / "performance.json").write_text(
            json.dumps(
                {
                    "loaded_actors": 400,
                    "tree_instance_count": 8000,
                    "peak_rain_particles": 20000,
                    "p95_frame_ms": 18.0,
                }
            ),
            encoding="utf-8",
        )
        # Minimal valid 1x1 PNG is not 1920x1080 — expect png BLOCKED/FAIL.
        tiny = (
            b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01"
            b"\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00"
            b"\x00\x01\x01\x00\x05\x18\xd8N\x00\x00\x00\x00IEND\xaeB`\x82"
        )
        for name in REQUIRED_PNGS:
            (root / name).write_bytes(tiny)

        report = evaluate_evidence_dir(root)
        assert report["overall"] in ("FAIL", "BLOCKED")
        assert any(g["gate"] == "landscape_valley" and g["status"] == "PASS" for g in report["gates"])
        print("self-test ok:", report["overall"], "gates", len(report["gates"]))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence_dir", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--out", type=Path, help="optional report JSON path")
    args = parser.parse_args(argv)
    if args.self_test:
        return _self_test()
    if not args.evidence_dir:
        parser.error("evidence_dir required unless --self-test")
    report = evaluate_evidence_dir(args.evidence_dir)
    text = json.dumps(report, indent=2)
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    print(text)
    return 0 if report["overall"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
