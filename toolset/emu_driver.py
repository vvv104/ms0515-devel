"""emu_driver - the emulator's, loaded from where the emulator is.

The driver of ms0515-cli over its stdio is the emulator's own tool
(tools/emu_driver.py in its repository); the toolset and the projects only
use it, so this stand-in loads it under the same name from the emulator
ms0515.py finds ($MS0515_DIR, else ../ms0515).
"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ms0515 import emulator  # noqa: E402

_spec = importlib.util.spec_from_file_location("_ms0515_emu_driver", emulator() / "tools" / "emu_driver.py")
_driver = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = _driver
_spec.loader.exec_module(_driver)

EmulatorDriver = _driver.EmulatorDriver
