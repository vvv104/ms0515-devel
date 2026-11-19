# TODO

Work the programs here owe to what the machine turned out to be.  The
emulator (`../ms0515`) is built from the findings of the FPGA rebuild
of the board (ms0515-fpga), and since its v1.21.0 (2026-11-07) it
keeps two of the machine's habits the programs here were written
without: the diskette turns, and the keyboard's auto-repeat is the
firmware's metronome code - the second done below, the first open.  (The frame rate stays 50 Hz, the rate the
kits' `CLOCK = 50.` counts - the timer interrupt is the frame sync; the
boards photographed carry the 72 Hz jumper, and whether the emulator
follows them is open.)  Each item says what
the machine does, what the program does, and what it takes.  Nothing
here is a defect of the emulator: the core follows the machine, and the
programs follow the core.

## The diskette's turn and the loaders

**The machine.**  A block asked for just after its ID has passed costs
a turn (200 ms); a request of several blocks reads them two sectors
apart within the turn.  Saboteur 2, which asks for its 85 blocks one at
a time with 23 ms of work between them, loads in 20 s on the machine
and in the emulator's window now; `ms0515-run` and the tests run
without the turn.

**The programs.**  The loaders here (FIST's `game_loader.py`, MANICM's,
UBIX's `boot/`) read through RT-11 or the ROM in requests of many
blocks and should be within the turn; none has been timed with it.

**What it takes.**  Run each program from the emulator's window (the
turn is on there) and note its load time beside the old one; a loader
that reads a block at a time with work between is the one to change,
if any does.

## Done: the metronome code and the keys (2026-11-19)

**The machine.**  The MS 7004 sends a key's code when it goes down and
nothing when it comes up; while keys are down it sends 264 (the LK201's
metronome) at its repeat rate for the newest of them - never the key's
code again (the firmware's `L_2B0`) - and, once that one is let go, for
the one under it, on the one delay counter the last press set (`L_1EF`,
`L_237`..`L_27D`; the emulator keeps that counter since its fix of the
fall-back, `docs/hardware/keyboard.md` there).  Nothing in the bytes
says which key a 264 repeats.  Both ROMs repeat the last key themselves.

- **The GCC toolset's library** (`toolset/gcc/machine/keys.c`): `ms_key`
  gives a 264 back as the last key's code, as the ROMs do; `ms_held`
  works on it as before.  STARS and the HELD example take their keys
  this way, through the library's own handler on vector 130 - not
  through the ROM - and were affected; `gcc_tests` and `stars_tests`
  pass.
- **MANICM** (`MS0515.MAC`, KEYS): a 264 repeats the last code; a
  repeat keeps the walk keys held and never touches a jump key (ONESHT),
  since after SPACE tapped on the move it is the walk key's as much as
  SPACE's.  SPACE tapped while P is held jumps once; SPACE held jumps
  once and not again on landing - the price, pinned by a test.
- **FIST** (`source/game_keys.py`, `source/game_text.py`): KSCAN compares
  with 264, where the code only refreshes the running timers (the chord
  rule); KGET, the settings screens' key reader, skips 264 - it took the
  repeat of a held key for the next key.  On the way: SETKEY, the key
  redefinition, kept its count and its names' pointer in registers PRTXT
  takes, and never ended; and the game's tests booted the vvv104 Omega on
  ROM-A, which the emulator ships as dumped since 2026-09-19, so the game
  never started there (11 of 21 failed) - they boot ROM-B now.  22 of 22.
- **Saper** (`source/gen_saper.py`): its three USART reads go through
  KREAD, which gives a 264 back as the last key's code; a held arrow
  walks the cursor.  `saper_tests` are new, and CI builds Saper and runs
  them (its text font is the ROM's now, not a Windows font file).

## Not affected, looked at

- **UBIX** takes its keys through the ROM (`kernel/tty.c`, `rom.s`),
  which understands 264: auto-repeat works as before.  Its tty is the
  ROM's.
- **Minesweeper, DECUS C, SL** read no USART of their own.
- **The keyboard's clock** (the timer's counter 0, which the emulator
  now listens to): the ROM programs it at boot and none of the programs
  here touches counters 0 and 1.
