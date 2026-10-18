# MS0515 development

**Read this first after compacting!**

Programs for the Elektronika MS 0515 and the toolset that builds them
(README.md).  This repository uses the emulator (`ms0515`) and the software
collection (`ms0515-software`), found beside it or by `MS0515_DIR` /
`MS0515_SOFTWARE` (`toolset/ms0515.py`, `toolset/cmake/Ms0515.cmake`,
`tools/deps.py`).  The dependency goes one way: nothing here may be needed
by the emulator, and nothing of the emulator is copied here - its binaries,
its library and its `tools/` are used where they are.

Before writing MACRO-11 or C for the machine read `docs/programming.md`
(the handbook) and `toolset/GOTCHAS.md`.

## Key rules
- All code, comments, and documentation must be in **English only**.
  MACRO-11 sources must be ASCII, comments included.
- **Never commit or push** without explicit user permission.  Commits to a
  feature branch are allowed without asking; commits to `main` and any push
  still require permission.
- **CI gates every merge**: open the PR, wait for CI green on the PR, merge,
  wait for CI green on `main`.  Fixes go through the feature branch.
- **Test-driven development**: after designing the interface, write the
  tests first, then implement.  Each program's tests live in its own
  `tests/`.
- **Revert failed attempts**; no dead code.
- **The language standard is declared once**, in `CMakeLists.txt`; the
  Conan profile's `compiler.cppstd` must match.
- **Zero compiler warnings**, and never suppress one - rewrite the call.
- **No vendored third-party sources**: dependencies come through Conan (the
  emulator's recipe says which) or from the original's own place, named by
  an environment variable (`WOTEF_DIR`, `MANICM_DIR`).
- **No machine-specific paths in tracked files**: before every commit
  `git grep -i "users[.]voron"` must return nothing.
- **No session links in the history**: commits and PR bodies carry the
  `Co-Authored-By:` trailer and the "Generated with Claude Code" credit,
  nothing else.
