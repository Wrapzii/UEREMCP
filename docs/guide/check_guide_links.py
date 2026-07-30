#!/usr/bin/env python3
"""Verify docs/guide markdown links and cited fixture paths.

Owner: WS-13 (docs/guide/**). Run without the editor::

    python docs/guide/check_guide_links.py

Exit 0 if clean, 1 on broken relative links or missing cited paths.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

GUIDE_DIR = Path(__file__).resolve().parent
REPO_ROOT = GUIDE_DIR.parents[1]

# Markdown links: [text](target) — skip http(s), mailto, anchors-only
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
# Explicit fixture/schema citations in backticks that look like repo paths
PATH_HINT_RE = re.compile(
    r"`((?:docs|schemas|Plugins|templates|tools|tests)/[^`\s]+)`"
)


def resolve_link(source: Path, target: str) -> Path | None:
    target = target.strip()
    if not target or target.startswith(("#", "http://", "https://", "mailto:")):
        return None
    # Drop fragment
    path_part = target.split("#", 1)[0]
    if not path_part:
        return None
    return (source.parent / path_part).resolve()


def collect_markdown_files() -> list[Path]:
    return sorted(GUIDE_DIR.glob("*.md"))


def check_markdown_links(errors: list[str]) -> int:
    checked = 0
    for md in collect_markdown_files():
        text = md.read_text(encoding="utf-8")
        for match in LINK_RE.finditer(text):
            dest = resolve_link(md, match.group(1))
            if dest is None:
                continue
            checked += 1
            if not dest.exists():
                rel = dest.relative_to(REPO_ROOT) if dest.is_relative_to(REPO_ROOT) else dest
                errors.append(f"{md.relative_to(REPO_ROOT)}: broken link -> {rel}")
    return checked


def looks_like_repo_file(rel: str) -> bool:
    """Skip MCP protocol paths (tools/call) and directory/glob mentions."""
    if "*" in rel or rel.endswith("/"):
        return False
    name = Path(rel).name
    # Require a file-looking leaf (extension or known extensionless scripts)
    if "." not in name and name not in {"AGENTS.md", "README", "CLAUDE.md"}:
        return False
    return True


def check_backtick_paths(errors: list[str]) -> int:
    """Paths cited in backticks under known roots must exist when they look like files."""
    checked = 0
    for md in collect_markdown_files():
        text = md.read_text(encoding="utf-8")
        for match in PATH_HINT_RE.finditer(text):
            rel = match.group(1)
            if not looks_like_repo_file(rel):
                continue
            path = REPO_ROOT / rel
            checked += 1
            if not path.exists():
                errors.append(
                    f"{md.relative_to(REPO_ROOT)}: missing cited path `{rel}`"
                )
    return checked


def check_required_guides(errors: list[str]) -> None:
    required = [
        "README.md",
        "agent-usage.md",
        "capability-reference.md",
        "limitations.md",
        "troubleshooting.md",
        "developer-setup.md",
        "template-authoring.md",
    ]
    for name in required:
        if not (GUIDE_DIR / name).is_file():
            errors.append(f"missing required guide: docs/guide/{name}")


def main() -> int:
    if not GUIDE_DIR.is_dir():
        print("error: docs/guide directory missing", file=sys.stderr)
        return 2

    errors: list[str] = []
    check_required_guides(errors)
    n_links = check_markdown_links(errors)
    n_paths = check_backtick_paths(errors)

    if errors:
        print(f"FAIL: {len(errors)} problem(s) ({n_links} links, {n_paths} path citations checked)")
        for err in errors:
            print(f"  {err}")
        return 1

    print(
        f"OK: docs/guide contract — {n_links} relative links, "
        f"{n_paths} path citations, required guides present"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
