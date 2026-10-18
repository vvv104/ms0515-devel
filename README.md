# MS 0515 development

Programs for the Электроника МС 0515 - the Soviet PDP-11 compatible
computer - written, ported or rebuilt here, and the toolset that builds
them on the host: the machine's own MACRO-11, LINK and compilers run by the
emulator's `ms0515-run`, GCC for `pdp11-aout`, and CMake rules over both.

It stands on two repositories of its own and changes neither:

- the emulator, <https://github.com/vvv104/ms0515> - its binaries run the
  tools and the programs, its library carries the programs' tests;
- the software collection, <https://github.com/vvv104/ms0515-software> -
  the systems and the tools' sources; what is built here and ships, ships
  there.

## Getting started

The three live side by side in one folder:

```
git clone https://github.com/vvv104/ms0515-devel.git
cd ms0515-devel
python tools/deps.py                  # ../ms0515 and ../ms0515-software, if missing
(cd ../ms0515/src && conan build . --build=missing)    # the emulator's package/
conan build . --build=missing         # the programs' tests, with the emulator inside
```

`tools/deps.py` fetches a missing neighbour at the ref `deps.toml` names
and leaves one that is there as it is.  Elsewhere they are named by
environment variables (`toolset/ms0515.py`, `toolset/cmake/Ms0515.cmake`):

| variable | what | when not set |
|---|---|---|
| `MS0515_DIR` | the emulator's repository | `../ms0515` |
| `MS0515_PACKAGE` | its binaries | `$MS0515_DIR/package` |
| `MS0515_SOFTWARE` | the software collection | `../ms0515-software` |
| `MS0515_GCC` | GCC for `pdp11-aout` (`toolset/gcc`) | on the `PATH` |

## The toolsets

Two ways to make a program for the machine, and nothing of either is kept
here twice.

**The machine's own tools**, run on the host one program at a time by the
emulator's `ms0515-run` (`toolset/build.py` with a project's `build.toml`,
or the CMake rules of `toolset/cmake/Rt11.cmake`).  The programs are the
software collection's, from its `software/development`:

| language | the tools | where they come from |
|---|---|---|
| MACRO-11 | `MACRO`, `LINK`, `LIBR`; `SYSMAC.SML`, `SYSLIB.OBJ`, `ODT.OBJ` | `LINK`, `LIBR` and the libraries are DEC's RT-11 V5.4, built from DEC's sources by `projects/rt11` and shipped to the collection; `MACRO` is the FODOS kit's, the one tool the V5.4 source kit has no source for |
| Pascal | `PAS1`, `PASLIB.OBJ`, `PAS1.OBJ`, the graphics `PASGRF.OBJ` | OMSI Pascal-1 as adapted for the machine, from its diskettes (`pascal/`), over the kits' system library (`fodos/`) |
| FORTRAN IV | `FORTRA`, `FORLIB.OBJ` | from the diskettes (`fortran/`), over the same system library |
| BASIC | `BASICO` (БЕЙСИК-ОМЕГА), `BASIC` (BASIC/RAFOS) | from the diskettes (`basic/`); interpreters, run with the program beside them |
| C | DECUS C: `CC`, `AS`, `CLIB.OBJ` | the SIG tape's sources (11SP59), rebuilt on the machine by `projects/decusc` and shipped to the collection (`decusc/`) |

**GCC on the host**, a cross compiler for `pdp11-aout`: GNU binutils 2.44
and GCC 15.2.0 from their released sources, C only, with one patch of
ours to the pdp11 backend and libgcc built for the T-11 (`-m10`: no EIS,
no FPP).  `toolset/gcc/build-toolchain.sh` builds it (on Windows, in WSL);
the runtime, the C library and the `.SAV` maker (`aout2sav.py`) are ours,
in `toolset/gcc/`, built by each project - see
[`toolset/gcc/README.md`](toolset/gcc/README.md).  STARS and UBIX are
made with it.

**On the host besides:** Python 3.11 or newer (the scripts, the
generators, `tomllib`), CMake 3.21+ and Ninja, Conan 2, and a C++20
compiler for the programs' tests, which link the emulator's library.

## What is where

| path | what |
|---|---|
| [`toolset/`](toolset/README.md) | the build: `build.py` (MACRO/LINK by `ms0515-run`), `cmake/` (the same as CMake rules), `gcc/` (the cross compiler, its runtime and the machine as a C library - [`c-api.md`](toolset/gcc/c-api.md)), [`GOTCHAS.md`](toolset/GOTCHAS.md) |
| `projects/<name>/` | a program each, with its sources, generators, README and its own `tests/` |
| [`docs/programming.md`](docs/programming.md) | the handbook: what a program running on the machine under RT-11 has to know - read it before writing for the machine |
| `CMakeLists.txt`, `conanfile.py` | the programs' tests, built together with the emulator (its Conan packages, its targets) |
| `deps.toml`, `tools/deps.py` | the repositories this one stands on |

This tree used to be `rt11_devel/` in the emulator's repository; its
history came along.
