# MS0515 development

**Read this first after compacting!**

Programs for the Elektronika MS 0515 and the toolset that builds them
(README.md).  This repository uses the emulator (`ms0515`) and the software
collection (`ms0515-software`), found beside it or by `MS0515_DIR` /
`MS0515_SOFTWARE` (`toolset/ms0515.py`, `toolset/cmake/Ms0515.cmake`,
`tools/deps.py`; `deps.toml` pins the refs CI takes).

Before writing MACRO-11 or C for the machine read `docs/programming.md`
(the handbook) and `toolset/GOTCHAS.md`.

## Key rules
- **This repository is the development of programs for the machine and
  nothing else**: the programs written, ported or rebuilt here, their
  tests, the toolset that builds them, the handbook.  It uses the
  emulator's artifacts - its binaries (`ms0515-run` runs the tools and the
  programs), its library (the programs' tests link it, the emulator built
  inside this build), its `tools/` (`emu_driver.py`, `pdp11_disasm.py`) -
  where they are, never copied here; and the collection's files (the
  systems, the tools' sources).  Neither of them may need anything from
  here.  What is the emulator's - emulating the machine - goes to the
  emulator; what is the collection's - the diskettes' files, their
  manuals, disk recovery - goes to the collection.  What is built here
  and ships to people ships through the collection (`projects/rt11/kit/
  ship_kit.py`, the games' and UBIX's images): this repository releases
  nothing itself.
- All code, comments, and documentation must be in **English only**.
  MACRO-11 sources must be ASCII, comments included.
- **Never commit or push** without explicit user permission.  Commits to a
  feature branch are allowed without asking; commits to `main` and any push
  still require permission.
- **CI gates every merge — `main` must stay clean.**  The mandatory order:
  open the PR → wait for CI green on the PR (one Linux job: the harnesses,
  Minesweeper by the machine's tools, the pdp11 compiler, the GCC
  examples, STARS and UBIX, every suite; what it built is the run's
  artifact; local green is NOT CI green - the owner builds on Windows,
  CI on Linux, and a difference is sorted out when it shows) →
  merge → wait for CI green on `main`.  Fixes discovered along the way go
  through the feature branch, never directly onto `main`.
- **Test-driven development**: after designing the interface, write the
  tests first, then implement; run them at each stage.  Each program's
  tests live in its own `tests/`, linked against the emulator's library -
  tests of the program, not of the emulator.
- **Revert failed attempts**: always roll back changes from unsuccessful
  approaches to avoid accumulating dead code and clutter.
- **The language standard is declared once**, in `CMakeLists.txt`
  (`CMAKE_CXX_STANDARD` / `CMAKE_C_STANDARD`) for this repository's
  targets; the emulator declares its own for its own.  Never repeat it per
  target.  The Conan profile's `compiler.cppstd` must match, since it is
  what dependencies are built against.
- **Zero compiler warnings**: the harnesses build with `/W4 /WX` and
  `-Wall -Wextra -Wpedantic -Werror`.  **Never suppress a warning** with a
  `#pragma`, a `_CRT_*` define or equivalents - rewrite the offending call.
- **No vendored third-party sources**: dependencies come through Conan (the
  emulator's recipe says which), and an original a port is made from (the
  game, its disassembly) is located through an environment variable
  (`WOTEF_DIR`, `MANICM_DIR`), never stored here.
- **No machine-specific paths in tracked files**: never a user's home
  directory, a drive letter or any absolute path of one machine - not as a
  default in code, not in docs, not in configs.  External resources are
  located through an environment variable with a repository-relative
  fallback (`projects/fist/source/wotef_dir.py`); examples in docs use
  `$PWD` or relative paths.  Before every commit `git grep -i "users[.]voron"`
  (and the equivalent for the machine at hand) must return nothing.
- **No session links in the history**: commit messages and pull request
  bodies carry the `Co-Authored-By:` trailer and the "Generated with Claude
  Code" credit, and nothing else.  A `Claude-Session:` line points into the
  owner's private Claude account - it opens nothing for anyone else and puts
  a session identifier into a public history.  Whatever attribution an
  assistant session is configured with, this rule wins.

## Project structure
```
CMakeLists.txt, conanfile.py — the build: the emulator's src/ as a subdirectory, the harnesses
deps.toml, tools/deps.py     — the neighbouring repositories and their pinned refs
docs/programming.md          — the handbook for programs on the machine
toolset/                     — build.py and cmake/ (MACRO/LINK by ms0515-run), gcc/ (GCC for
                               pdp11-aout: the compiler's build, runtime, examples, tests),
                               ms0515.py (where the neighbours are), emu_driver.py (a loader for
                               the emulator's), system/ (the folder system the games' tests boot),
                               tests/ (pytest), GOTCHAS.md
projects/<name>/             — one program each, with its sources, README and tests/: fist, manicm,
                               minesweeper, saper, stars, crypt (branch), unix (UBIX), rt11 (the
                               RT-11 kit from DEC's sources), sl, decusc
tools/                       — this repository's own scripts (deps.py, rt11_handler.py)
```
