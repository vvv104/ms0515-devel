"""Where ms0515-run is taken from: MS0515_RUN, else the emulator's binaries,
named as the host names an executable."""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ms0515                                            # noqa: E402


def test_run_tool_from_ms0515_run(monkeypatch, tmp_path):
    tool = tmp_path / "bin" / "ms0515-run"
    monkeypatch.setenv("MS0515_RUN", str(tool))
    assert ms0515.run_tool() == tool


def test_run_tool_in_the_package(monkeypatch, tmp_path):
    monkeypatch.delenv("MS0515_RUN", raising=False)
    monkeypatch.setenv("MS0515_PACKAGE", str(tmp_path))
    name = "ms0515-run.exe" if os.name == "nt" else "ms0515-run"
    assert ms0515.run_tool() == Path(tmp_path) / name
