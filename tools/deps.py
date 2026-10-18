"""deps.py - the repositories this one stands on, beside it.

    python tools/deps.py           each of deps.toml found or fetched
    python tools/deps.py --check   only say where each is; 1 if one is missing

A dependency is taken from its environment variable (deps.toml's `env`)
when that is set, else from the folder of its name beside this repository.
A folder that is there is used as it is - checked out at whatever its owner
has there, never touched; one that is not is cloned from `url` and checked
out at `ref`.  So a newcomer runs this once after cloning, and a machine
that already has the repositories side by side changes nothing.  CI runs
it the same way.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def git(*args: str, cwd: Path | None = None) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def place(name: str, dep: dict) -> tuple[Path, str]:
    """Where the dependency is to be, and what says so."""
    value = os.environ.get(dep["env"])
    if value:
        return Path(value), f"${dep['env']}"
    return ROOT.parent / name, "beside this repository"


def fetch(where: Path, dep: dict) -> None:
    git("clone", "--quiet", dep["url"], str(where))
    git("checkout", "--quiet", dep["ref"], cwd=where)


def main() -> int:
    check = "--check" in sys.argv[1:]
    deps = tomllib.loads((ROOT / "deps.toml").read_text(encoding="utf-8"))
    missing = 0
    for name, dep in deps.items():
        where, why = place(name, dep)
        if where.is_dir():
            head = git("rev-parse", "--short", "HEAD", cwd=where) if (where / ".git").exists() else "not a git checkout"
            print(f"{name}: {where} ({why}, at {head})")
            continue
        if check:
            print(f"{name}: missing at {where} ({why})")
            missing += 1
            continue
        print(f"{name}: fetching {dep['url']} at {dep['ref']} into {where}")
        fetch(where, dep)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
