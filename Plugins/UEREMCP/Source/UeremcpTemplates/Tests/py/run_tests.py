#!/usr/bin/env python3
"""Run WS-15 template library tests."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> int:
    steps = [
        [sys.executable, str(HERE / "validate_templates.py")],
        [sys.executable, str(HERE / "test_templates.py")],
    ]
    for cmd in steps:
        print(f"RUN {' '.join(cmd)}")
        completed = subprocess.run(cmd, check=False)
        if completed.returncode != 0:
            return completed.returncode
    print("OK  all UeremcpTemplates python tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
