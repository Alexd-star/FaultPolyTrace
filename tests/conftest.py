# -*- coding: utf-8 -*-
"""Project-wide pytest setup for a stable Windows temporary directory."""

import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PYTEST_TEMP_ROOT = PROJECT_ROOT / ".pytest_tmp"
PYTEST_TEMP_ROOT.mkdir(parents=True, exist_ok=True)
os.environ.setdefault("PYTEST_DEBUG_TEMPROOT", str(PYTEST_TEMP_ROOT))
