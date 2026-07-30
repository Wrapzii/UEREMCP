#!/usr/bin/env python3
"""Out-of-editor contract checks for UeremcpSecurity (WS-12).

Validates docs and header API surface without Unreal. C++ behaviour is covered by
UEREMCP.Security.* automation tests when the plugin is built.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = Path(__file__).resolve().parents[5]
PUBLIC = ROOT / "Public"


def main() -> int:
    errors: list[str] = []

    security_md = REPO / "docs" / "SECURITY.md"
    if not security_md.is_file():
        errors.append("docs/SECURITY.md missing")
    else:
        body = security_md.read_text(encoding="utf-8")
        for heading in ("Operator", "Agent", "UnrealWatch"):
            if heading not in body:
                errors.append(f"docs/SECURITY.md missing '{heading}' section")

    required_headers = {
        "UeremcpPathPolicy.h": ["ValidateSoftPath", "ValidateFilesystemPath"],
        "UeremcpPermissionPolicy.h": ["Evaluate"],
        "UeremcpMutatorQueue.h": ["TryAcquire", "Release", "CancelQueued", "PendingCount", "IsImplemented"],
        "UeremcpAuditLog.h": ["Append", "AuditDirectory", "DailyLogFileName", "IsImplemented"],
        "UeremcpSecuritySettings.h": ["bAllowUnsafe", "AuditRetentionDays"],
    }
    for name, symbols in required_headers.items():
        path = PUBLIC / name
        if not path.is_file():
            errors.append(f"missing header {name}")
            continue
        text = path.read_text(encoding="utf-8")
        for symbol in symbols:
            if symbol not in text:
                errors.append(f"{name} missing symbol {symbol}")

    audit = (PUBLIC / "UeremcpAuditLog.h").read_text(encoding="utf-8") if (PUBLIC / "UeremcpAuditLog.h").is_file() else ""
    if "YYYY-MM-DD.jsonl" not in audit and "DailyLogFileName" not in audit:
        errors.append("audit log daily JSONL contract not documented in header")

    proposal = REPO / "docs" / "proposals" / "ws-12-register-security-module.md"
    if not proposal.is_file():
        errors.append("missing ws-12-register-security-module.md proposal")

    if errors:
        for err in errors:
            print(f"error: {err}", file=sys.stderr)
        return 1

    print("UeremcpSecurity contract checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
