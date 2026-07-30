#!/usr/bin/env python3
"""Check that changed files fall inside the calling workstream's owned paths.

Owner: WS-01. See docs/WORK_ALLOCATION.md for the ownership map this encodes.

This is *advisory*, not a lock. Its job is to catch the accident -- an agent editing
a shared schema or another workstream's module without noticing -- not to stop anyone
determined. With fifteen agents on one repo, the accident is the common case.

Usage::

    python tools/check_ownership.py --ws WS-07
    python tools/check_ownership.py --ws WS-07 --base main
    python tools/check_ownership.py --ws WS-07 --paths a/b.cpp c/d.h

By default it inspects the working tree (staged + unstaged + untracked). Pass --base
to compare against a branch instead.

Exit code 0 if clean, 1 if any changed path is outside the workstream's ownership.
"""

from __future__ import annotations

import argparse
import fnmatch
import subprocess
import sys

# Ownership map. Globs are matched against repo-relative POSIX paths.
# Keep in sync with docs/WORK_ALLOCATION.md -- that document is authoritative.
OWNERSHIP: dict[str, list[str]] = {
    "WS-01": [
        "AGENTS.md", "CLAUDE.md",
        "docs/adr/**", "docs/GROUNDED_FACTS.md", "docs/WHY.md",
        "docs/WORK_ALLOCATION.md", "docs/RESEARCH_PROTOCOL.md", "docs/ROADMAP.md",
        "docs/RISK_REGISTER.md", "docs/POC_ACCEPTANCE.md", "docs/CAPABILITY_CATALOG.md",
        "docs/SWARM_LAUNCH.md", "docs/BACKLOG.md", "docs/BENCHMARK_PROTOCOL.md",
        "docs/COVERAGE_PLAN.md", "docs/TOOL_ROUTER.md", "docs/VISUAL_CAPTURE_PROTOCOL.md",
        "README.md", "Scripts/**",
        "docs/research/README.md", "docs/research/RB-01-*",
        "schemas/common/**", "schemas/envelope/**", "schemas/graph/**",
        "schemas/template-library/**", "schemas/examples/**",
        "schemas/domains/environment/**",
        "Plugins/UEREMCP/Source/UeremcpEnvironment/**",
        "tools/check_ownership.py", "tools/check_tool_names.py",
        "tools/dump_tool_registry.py", "tools/gen_focus_config.py",
        "tools/route_prototype.py", "tools/registry_snapshot.json",
    ],
    "WS-02": ["docs/audit/**", "docs/research/RB-02-*", "docs/research/RB-15-*"],
    "WS-03": [
        "Plugins/UEREMCP/UEREMCP.uplugin", "Plugins/UEREMCP/README.md",
        "Plugins/UEREMCP/Source/UeremcpCore/**", "docs/research/RB-03-*",
    ],
    "WS-04": ["Plugins/UEREMCP/Source/UeremcpTransport/**", "docs/research/RB-04-*"],
    "WS-05": [
        "Plugins/UEREMCP/Source/UeremcpProtocol/**", "schemas/batch/**",
        "schemas/domains/_shared/**", "tools/validate_schemas.py",
    ],
    "WS-06": [
        "Plugins/UEREMCP/Source/UeremcpBlueprint/**", "schemas/domains/blueprints/**",
        "docs/research/RB-05-*",
    ],
    "WS-07": [
        "Plugins/UEREMCP/Source/UeremcpNiagara/**", "schemas/domains/niagara/**",
        "docs/research/RB-07-*",
    ],
    "WS-08": [
        "Plugins/UEREMCP/Source/UeremcpMaterial/**", "schemas/domains/materials/**",
        "docs/research/RB-08-*", "docs/research/RB-11-*",
    ],
    "WS-09": [
        "Plugins/UEREMCP/Source/UeremcpGameplay/**", "schemas/domains/gameplay/**",
        "docs/research/RB-12-*",
    ],
    "WS-10": [
        "Plugins/UEREMCP/Source/UeremcpAnimation/**", "schemas/domains/animation/**",
        "docs/research/RB-09-*",
    ],
    "WS-11": [
        "Plugins/UEREMCP/Source/UeremcpValidation/**", "tests/**",
        "docs/research/RB-06-*", "docs/research/RB-14-*",
    ],
    "WS-12": [
        "Plugins/UEREMCP/Source/UeremcpSecurity/**", "docs/SECURITY.md",
        "docs/research/RB-13-*",
    ],
    "WS-13": ["docs/guide/**"],
    "WS-14": ["docs/reviews/**"],
    "WS-15": [
        "Plugins/UEREMCP/Source/UeremcpTemplates/**", "templates/**",
        "schemas/domains/templates/**", "docs/research/RB-10-*",
    ],
}

# Anyone may write here. Proposals are the escape hatch for "I need something I do
# not own" (AGENTS.md rule 3), so they must never be blocked.
UNIVERSAL = ["docs/proposals/**"]


def matches(path: str, patterns: list[str]) -> bool:
    for pattern in patterns:
        if fnmatch.fnmatch(path, pattern):
            return True
        # fnmatch treats '**' as a plain wildcard, which is what we want for prefix
        # matching, but 'a/**' should also match 'a' itself.
        if pattern.endswith("/**") and path == pattern[:-3]:
            return True
    return False


def changed_paths(base: str | None) -> list[str]:
    if base:
        cmd = ["git", "diff", "--name-only", f"{base}...HEAD"]
        out = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if out.returncode != 0:
            sys.stderr.write(f"error: git diff failed: {out.stderr.strip()}\n")
            raise SystemExit(2)
        return [p for p in out.stdout.splitlines() if p]

    paths: set[str] = set()
    for cmd in (
        ["git", "diff", "--name-only"],
        ["git", "diff", "--name-only", "--cached"],
        ["git", "ls-files", "--others", "--exclude-standard"],
    ):
        out = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if out.returncode == 0:
            paths.update(p for p in out.stdout.splitlines() if p)
    return sorted(paths)


def owner_of(path: str) -> str | None:
    for ws, patterns in OWNERSHIP.items():
        if matches(path, patterns):
            return ws
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ws", required=True, help="Your workstream, e.g. WS-07")
    parser.add_argument("--base", help="Compare against this ref instead of the working tree")
    parser.add_argument("--paths", nargs="*", help="Check these paths instead of asking git")
    args = parser.parse_args()

    ws = args.ws.upper()
    if ws not in OWNERSHIP:
        sys.stderr.write(f"error: unknown workstream '{ws}'. Known: {', '.join(sorted(OWNERSHIP))}\n")
        return 2

    paths = args.paths if args.paths else changed_paths(args.base)
    if not paths:
        print("no changes to check")
        return 0

    mine = OWNERSHIP[ws] + UNIVERSAL
    violations: list[tuple[str, str]] = []
    unowned: list[str] = []

    for path in paths:
        if matches(path, mine):
            continue
        owner = owner_of(path)
        if owner is None:
            unowned.append(path)
        else:
            violations.append((path, owner))

    for path in unowned:
        print(f"WARN  {path}  (unowned -- belongs to WS-01 until assigned; propose, do not squat)")

    if violations:
        sys.stderr.write(f"\n{ws} modified {len(violations)} path(s) it does not own:\n\n")
        for path, owner in violations:
            sys.stderr.write(f"  {path}\n      owned by {owner}\n")
        sys.stderr.write(
            "\nAGENTS.md rule 3: write docs/proposals/%s-<topic>.md instead, and keep\n"
            "working on what you do own. If you are seeing this, you are probably about\n"
            "to overwrite another agent's work.\n" % ws.lower()
        )
        return 1

    print(f"OK  {len(paths)} changed path(s), all within {ws}'s ownership")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
