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
        for heading in (
            "Operator",
            "Agent",
            "UnrealWatch",
            "FUeremcpMutatingDispatch",
            "Not a toolset",
        ):
            if heading not in body:
                errors.append(f"docs/SECURITY.md missing '{heading}' section/text")
        if "RegisterToolsetClass" not in body:
            errors.append("docs/SECURITY.md must document absence of RegisterToolsetClass")
        if "R-07 stays open" not in body and "R-07 remains" not in body:
            errors.append("docs/SECURITY.md must state R-07 remains open until domain adoption")

    required_headers = {
        "UeremcpSecurity.h": ["UeremcpSecurityDomainAdoption.h", "UeremcpPermissionPolicy.h"],
        "UeremcpSecurityDomainAdoption.h": [
            "PreferredGateHeader",
            "PredictedDeletedForDestructiveReplace",
            "ValidateWriteSoftPath",
            "UeremcpMutatingDispatch.h",
        ],
        "UeremcpPathPolicy.h": ["ValidateSoftPath", "ValidateFilesystemPath"],
        "UeremcpPermissionPolicy.h": ["Evaluate", "IsUnsafeAction"],
        "UeremcpMutatorQueue.h": [
            "TryAcquire",
            "Release",
            "CancelQueued",
            "PendingCount",
            "IsImplemented",
        ],
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

    adoption_cpp = ROOT / "Private" / "UeremcpSecurityDomainAdoption.cpp"
    if not adoption_cpp.is_file():
        errors.append("missing UeremcpSecurityDomainAdoption.cpp")

    audit = (
        (PUBLIC / "UeremcpAuditLog.h").read_text(encoding="utf-8")
        if (PUBLIC / "UeremcpAuditLog.h").is_file()
        else ""
    )
    if "YYYY-MM-DD.jsonl" not in audit and "DailyLogFileName" not in audit:
        errors.append("audit log daily JSONL contract not documented in header")

    for proposal_name in (
        "ws-12-register-security-module.md",
        "ws-12-niagara-mutating-dispatch-handoff.md",
        "ws-12-material-mutating-dispatch-handoff.md",
    ):
        proposal = REPO / "docs" / "proposals" / proposal_name
        if not proposal.is_file():
            errors.append(f"missing {proposal_name} proposal")

    module_cpp = (ROOT / "Private" / "UeremcpSecurityModule.cpp").read_text(encoding="utf-8")
    if "UToolsetRegistry::RegisterToolsetClass" in module_cpp or "RegisterToolsetClass(" in module_cpp:
        errors.append("UeremcpSecurityModule must not call RegisterToolsetClass")

    if errors:
        for err in errors:
            print(f"error: {err}", file=sys.stderr)
        return 1

    print("UeremcpSecurity contract checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
