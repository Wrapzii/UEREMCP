#!/usr/bin/env python3
"""Run UeremcpProtocol outside-editor unit tests.

Usage (from repo root)::

    python Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py/run_tests.py

Or::

    python -m unittest discover -s Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py -v
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
