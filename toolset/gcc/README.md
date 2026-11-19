# GCC for the MS 0515

Modern C for the machine: GCC built as a cross compiler for the target
`pdp11-aout`, with the processor's instruction set (`-m10`: the T-11's,
no EIS, no FPP), a runtime that makes its output an RT-11 program, and a
CMake module that makes a C program an ordinary CMake project.  The
machine takes no part in the build - everything is compiled on the host,
as `ms0515-disk` works - and only the finished `.SAV` runs on it, by
`ms0515-run` or from a diskette.

```
toolset/gcc/
├── build-toolchain.sh   binutils + gcc for pdp11-aout from their sources, patched
├── gcc-pdp11-cmpsi.patch  the fix of the backend's comparison of longs
├── patch-gcc.py         what made the patch; remakes it for another GCC
├── pdp11-rt11.cmake     the CMake toolchain file (Rt11Gcc.cmake names it)
├── aout2sav.py          the linked a.out as a .SAV: block 0, bss, stack
├── rt/                  the runtime, assembled with each project
│   ├── crt0.s           _start: main() then .EXIT; __main, exit, rt11_memtop
│   ├── arith.s          the 16- and 32-bit multiply and divide, __xorhi3
│   ├── emt.s            RT-11's requests as C functions
│   ├── files.c          the files: .LOOKUP, .READW, .READ and .WAIT, .ENTER, .WRITW, .CLOSE
│   └── fx.s, fx.c       fixed point cut to size: fx_div8, fx_mul, fx_sin, fx_cos
├── libc/                stdio over the console, string, ctype, stdlib, the heap
├── machine/             the screen, the clock, the keyboard, the joystick, the speaker
├── include/             rt11.h, ms0515.h, fx.h, stdio.h, stdlib.h, string.h, ctype.h
├── examples/            HELLO, ARITH, LIBC, LIBC2, INPUT, FX, MACHINE, HELD, PORTS, DRAW,
│                        STREAM, FILES, DEEP (the runtime checked), CMPLONG, LONGP0,
│                        LONGPS, MULCOST, BSSZERO, VARSHIFT (the compiler's faults,
│                        fixed), CALC (the benchmark)
└── tests/               the examples run by ms0515-run's machine (doctest)
```

## The compiler

```
toolset/gcc/build-toolchain.sh [<prefix>]      default ~/pdp11-gcc
```

downloads binutils 2.44 and GCC 15.2.0, builds them for `pdp11-aout` (C
only) and installs `pdp11-aout-gcc`, `-as`, `-ld`, `-objdump` and the
rest under `<prefix>/bin`.  libgcc is compiled with `-m10` too, so that
nothing it brings in executes an instruction the processor lacks.  The
host needs gcc, make, bison, flex, texinfo and the GMP/MPFR/MPC
development packages; the build takes a quarter of an hour.  On Windows
it runs in WSL (below).

Set `MS0515_GCC` to the prefix, or put `<prefix>/bin` on the PATH.

## On Windows

The compiler is built and used in WSL; the emulator stays the Windows
one, and a program built in WSL is run there by `ms0515-run.exe`.  This
way was checked from a clean install on 2026-11-19: Windows 11, WSL 2.6,
Ubuntu 24.04.

1. WSL with Ubuntu, from PowerShell:

   ```
   wsl --install -d Ubuntu-24.04
   ```

   The first start of Ubuntu asks for a user name and a password.  That
   user is WSL's default one, and the Windows drives in `/mnt/c` are his;
   CMake builds in a folder there only as him (as another user `cmake`
   stops at `configure_file: Operation not permitted`).

2. The packages, in Ubuntu:

   ```
   sudo apt update
   sudo apt install -y build-essential patch bison flex texinfo \
       libgmp-dev libmpfr-dev libmpc-dev wget xz-utils cmake ninja-build python3
   ```

3. The repositories are the Windows ones (Getting started in the
   repository's README); WSL sees them under `/mnt/c/...`.  The shell
   scripts keep LF line ends on a Windows checkout (`.gitattributes`), so
   they run from there.  The compiler goes to the Linux home:

   ```
   /mnt/c/<path>/ms0515-devel/toolset/gcc/build-toolchain.sh
   echo 'export MS0515_GCC=~/pdp11-gcc' >> ~/.bashrc && source ~/.bashrc
   ```

4. The examples, in WSL, and one of them run by the Windows emulator:

   ```
   cd /mnt/c/<path>/ms0515-devel
   cmake -S toolset/gcc/examples -B toolset/gcc/examples/build -G Ninja
   cmake --build toolset/gcc/examples/build
   cd toolset/gcc/examples/build/sav
   /mnt/c/<path>/ms0515/package/ms0515-run.exe HELLO
   ```

   prints `HELLO FROM GCC`.

5. The tests run on the Windows side: `conan build . --build=missing` in
   `ms0515-devel` builds `gcc_tests`, which takes the `.SAV`s from
   `toolset/gcc/examples/build/sav` (Tests, below).

## A project

```cmake
cmake_minimum_required(VERSION 3.21)
include(<repository>/toolset/cmake/Rt11Gcc.cmake)
project(myprog LANGUAGES C ASM)

rt11_c_library(NAME world SOURCES world.c sectors.c INCLUDE include)
rt11_c_program(NAME MYPROG SOURCES myprog.c helpers.s LIBRARIES world [STACK 2048])
```

The include stands before `project()`: it names the toolchain file, which
`project()` reads.  `SOURCES` is everything compiled and linked into the
program - `.c` through the compiler, `.s` through the assembler - and
`LIBRARIES` the project's own libraries, each an `rt11_c_library` of its
sources with its headers' folders; the runtime, the C library and the
machine (`librt11.a`) and libgcc are linked without being named, and the
linker takes from a library only the objects a program refers to.  A
program's data files are not linked: they lie beside the `.SAV` and are
read by block (`rt11.h`).  `cmake -S . -B build -G Ninja && cmake --build
build` leaves `build/sav/MYPROG.SAV`, and `ms0515-run MYPROG` there runs
it.  `myprog` is the a.out target (`build/myprog.out`) for
`pdp11-aout-objdump -d` when the code is in question, and an ordinary
CMake target for `target_compile_definitions` and the rest.

## What a program finds

The reference of it all, function by function, is `c-api.md` in this folder; this
is the shape.

- `int` and pointers are 16 bits, `long` 32, `long long` 64; `-m10`
  makes every `*`, `/` and `%` a call into `rt/arith.s` - the 16-bit
  ones and the 32-bit ones too, since libgcc has no 16-bit helpers for
  pdp11 and its 32-bit division is miscompiled (below).  There is no
  floating point.
- The C library (`libc/`, `include/`): `stdio.h` is the console -
  `putchar`, `getchar`, `puts`, `printf` with `d i u x X o c s %`, the
  flags `-` and `0`, a width, a precision for `s` and `l` for a long,
  `sprintf`, `snprintf`; `'\n'` goes out as CR LF and comes in as the
  LF after the CR.  `string.h` whole, `ctype.h` for ASCII, `stdlib.h`
  with `atoi`, `strtol`, `abs`, `rand`, `malloc` and company - the heap
  is the memory above the stack, taken from the monitor by `.SETTOP`.
  `rt11.h` has the monitor's requests themselves - `.TTYOUT`, `.TTYIN`,
  `.PRINT`, `.SETTOP`, `exit()` - and `main()`'s return value is the
  program's end.
- Files, a block of 512 bytes at a time, as RT-11 gives them (`rt11.h`,
  `rt/files.c`): a name in RAD50, `.LOOKUP` once at the start (it needs
  the USR), `.READW` during the run, `.READ` left to the disk's handler
  and `.WAIT`ed for, `.ENTER` and `.WRITW` for a file of the program's
  own, `.CLOSE`; the error's code from byte 052.  The monitor lies
  behind the VRAM window, so a program with the screen on closes the
  window round a request (`ms_window`).  No stdio over them yet:
  `fopen` and friends are not there, a game streams its data by block.
- Fixed point cut to size (`fx.h`): `fx_div8` for a quotient of eight
  bits in eight steps - the projection's division - `fx_mul` for 8.8
  products over the full 32-bit product, `fx_sin` and `fx_cos` from a
  table of a quarter turn, for the rays and the turns of a 3D picture.
- Textures and sprites drawn scaled (`tex.h`): a texture's column
  stretched to its height on the screen, clipped above and below, as
  a wall stands at its distance; a sprite scaled both ways with its
  empty texels left unwritten.  What a 3D view is painted with.
- The machine (`machine/`, `ms0515.h`): what a program does outside
  the monitor, as `docs/programming.md` says it and the ports do it -
  the screen in 320x200 colour through the VRAM window at 0100000 with
  the border, the attributes and the pixels; the frame interrupt as a
  clock; the keyboard taken off the ROM's vector into a ring, with the
  held-key timers the keyboard's lack of release codes calls for; the
  joystick port; the speaker bit; the extended memory banks; columns,
  boxes, images and text in the ROM's own font, found in whichever ROM
  is there.  Everything is C over the registers
  but the two interrupt handlers (RTI) and the PSW's two instructions.
  `examples/machine.c` uses all of it; STARS is built on it.
- The program lies from 01000: text, data, bss, then the stack's room
  (`STACK`, 1024 bytes when not said).  Above the high limit the memory
  is the program's to take by `rt11_settop()`.  The video window and the
  dispatcher are the program's own business, as in MACRO-11
  (`docs/programming.md`).
- The objects are a.out, not RT-11's: a program is linked whole by
  GCC's `ld`, there is no linking with MACRO-11 objects or SYSLIB.
  Assembler of the project's own goes in GNU as syntax beside the C
  (`examples/tick.s`): `$` for an immediate, `*$` for an absolute
  address (MACRO's `@#`), a leading `0` for octal, every C symbol with a
  leading underscore, the arguments on the stack at `2(sp)`, `4(sp)`...,
  the result in r0, r0 and r1 scratch, r2..r5 kept.

## The patches, and why

GCC's pdp11 backend is patched once, by `gcc-pdp11-cmpsi.patch`, which
`build-toolchain.sh` applies before the build; `patch-gcc.py` made it and
remakes it for another GCC.  It makes six changes.  Three of them are
there because of the MS 0515's processor: the KR1807VM1 is a T-11, which
has no EIS - no `MUL`, `DIV`, `ASH`, `ASHC` - and no FPP, so the code is
generated with `-m10`, a path of the backend few programs walk.  The
other three mend faults of the backend that any PDP-11 target has,
met in our own code.  Each has an example whose test fails on a compiler
without the patch.

| change | why | where it showed | test |
|---|---|---|---|
| shifts by a constant written out | the T-11 has no `ASH` | `x >> 5` in every rotation | - |
| multiply and divide priced as calls | the T-11 has no `MUL` and `DIV` | UBIX | MULCOST |
| shift by a variable count | the loop that stands for `ASH` | UBIX | VARSHIFT |
| signed comparison of longs | a fault of the backend | the C library's 32-bit arithmetic | CMPLONG |
| long read through a pointer | a fault of the backend | UBIX | LONGP0, LONGPS |
| zeroed variables in the bss | a fault of the backend | a program's file carrying its zeros | BSSZERO |

**Shifts by a constant.**  Without `ASH` the backend writes a shift by up
to 3 as single shifts and a longer one as a loop - four words, three
instructions a step - so `x >> 5` cost seventeen instructions.  The patch
writes shifts out up to 8 when speed is wanted (`-O2`) and keeps the loop
from 4 under `-Os`.  This is speed only: the loop was right, and nothing
fails without the change.

**The price of a multiply.**  The backend priced a multiply as one `MUL`
- under `-Os` as any one instruction - when without EIS it is a call of
`__mulhi3`, some eighty instructions.  `-Os` therefore rewrote a walk
over a table by a pointer as a step count times the element's size, two
calls a step (UBIX's buffer cache).  Multiply, divide and remainder are
priced as the calls they are now; `examples/mulcost.c` reads its own walk
for a call and finds none.

**A shift by a variable count.**  Without `ASH` such a shift is a loop
counting down a copy of the count, and the pattern declared the copy
clobbered by `match_dup`; copy propagation put the count itself in the
copy's place, so a loop's index shifted by came out of the shift as 0
and the loop round it never ended (UBIX's `ls -l`, its permission bits).
The clobbered register is an output of its own now, the count tied to
it; `examples/varshift.c` hangs without the fix.

**A signed comparison of two longs.**  When their high words are equal
the backend compares the low words with the same signed branch, so a low
word with bit 15 set reads as negative: `40000L > 5L` is false, `32768L
>= 0` too.  Comparisons whose high words differ, unsigned comparisons and
tests for equality were right.  libgcc's own 32-bit division falls into
it (its `__udivmodsi4` tests the divisor's sign).  After the last compare
of a two- or four-word comparison, five words now set N and V from C, so
that a signed branch reads the unsigned order of the low words and an
unsigned one sees what it saw; `examples/cmplong.c` is the proof.

**A long read through a pointer** into the register pair the pointer was
in - the pointer in `r0`, the long for `r0:r1` - was loaded high word
first, over the pointer, and the low word read through what had just come
in (`mov 4(r0),r0` / `mov 6(r0),r1`): every such read at `-O0`, and at
`-Os` where a function gave the pointer back (UBIX's kernel, its file
system).  The move loads the address register last now;
`examples/longptr.c`, built at `-O0` (LONGP0) and at `-Os` (LONGPS), is
the proof.

**Zeroed variables** were written into `.data` as `.=.+N`, so a
program's file carried its zeros; they go to `.bss` now, and
`examples/bsszero.c` finds its arrays between `_edata` and `_end`.  A
`.SAV` holds its memory whole, bss and all; the gain is for programs
loaded by a loader that clears the bss, as UBIX's kernel.

Besides the patch, what the toolset does for the processor without
touching GCC's sources:

- libgcc is built with `-m10` (`build-toolchain.sh`), so that nothing it
  brings in executes an instruction the T-11 lacks; the projects are
  compiled with `-m10` too (`pdp11-rt11.cmake`).
- `rt/arith.s` carries the 16-bit multiply, divide and remainder, which
  libgcc has none of for pdp11, and the 32-bit ones, libgcc's division
  being the one the comparison fault broke.
- `-m10` also means the PDP-11/10, which had no `XOR` and no `SOB`; the
  T-11 has both, but GCC calls a `__xorhi3` for `^`, which `rt/arith.s`
  gives it, and writes no `SOB`.
- The output is a.out; `aout2sav.py` makes it an RT-11 `.SAV`.

## How fast

The CALC benchmark of 2026-09-25 (primes below 10000 by trial division
without multiply, divide or shift, timed by the machine's own 50 Hz
clock):

| MACRO-11 | GCC -O2 | DECUS C (`register`) | DECUS C | PAS1 | FORTRAN IV |
|---|---|---|---|---|---|
| 25.0 s | 29.3 s | 47.2 s | 76.1 s | 107.0 s | 135.2 s |

`-O3` is slower (41 s: the unrolling costs more than it saves), `-Os`
and `-O1` come to 33 s.

## Tests

`gcc_tests`, built by this repository's `CMakeLists.txt`,
runs the examples' `.SAV`s from `examples/build/sav` - or
`--gcc-sav=<folder>` - on ms0515-run's machine and checks what they
print; without them it skips, unless the folder was named, when a
missing program fails.  CI (`.github/workflows/ci.yml`) builds the
compiler on one Linux job, cached by the script and the patch, builds
the examples and runs the suite that way.  Build the examples first:

```
cmake -S toolset/gcc/examples -B toolset/gcc/examples/build -G Ninja
cmake --build toolset/gcc/examples/build
```
