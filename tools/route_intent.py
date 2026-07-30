#!/usr/bin/env python
"""Entry point shim — delegates to tools/intent_router/router.py."""
from __future__ import annotations
import runpy
import os
runpy.run_path(os.path.join(os.path.dirname(__file__), "intent_router", "router.py"), run_name="__main__")
