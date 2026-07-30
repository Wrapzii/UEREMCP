#!/usr/bin/env python3
"""Run offline UeremcpBlueprint tests (no editor required).

Usage (from repo root)::

    python Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/py/run_tests.py
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


def main() -> int:
    here = Path(__file__).resolve().parent
    sys.path.insert(0, str(here))
    suite = unittest.defaultTestLoader.discover(str(here), pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
