"""ms0515 - where this repository finds what it stands on.

The emulator (https://github.com/vvv104/ms0515) and the software
collection (https://github.com/vvv104/ms0515-software) are repositories of
their own, and everything here only uses them: the emulator's binaries run
the machine's tools and the programs, its tools/ drive ms0515-cli, the
collection gives the systems and the tools' sources.  They are found the
same way everywhere, each time asked (a test may set the variables):

    MS0515_DIR       the emulator's repository; ../ms0515 beside this one
                     when not set (tools/deps.py puts it there)
    MS0515_PACKAGE   its binaries; MS0515_DIR/package when not set (the
                     emulator's own build puts them there)
    MS0515_SOFTWARE  the software collection; ../ms0515-software beside
                     this one when not set
"""
from __future__ import annotations

import os
from pathlib import Path

DEVEL = Path(__file__).resolve().parents[1]


def _from_env(name: str, default: Path) -> Path:
    value = os.environ.get(name)
    return Path(value) if value else default


def emulator() -> Path:
    """The emulator's repository; a clear stop when it is not there."""
    root = _from_env("MS0515_DIR", DEVEL.parent / "ms0515")
    if not (root / "src" / "CMakeLists.txt").is_file():
        raise SystemExit(f"no emulator repository at {root}: set MS0515_DIR or run tools/deps.py")
    return root


def package() -> Path:
    """The emulator's binaries (and their assets/)."""
    return _from_env("MS0515_PACKAGE", _from_env("MS0515_DIR", DEVEL.parent / "ms0515") / "package")


def software() -> Path:
    """The software collection (not checked: its users say what they need of it)."""
    return _from_env("MS0515_SOFTWARE", DEVEL.parent / "ms0515-software")
