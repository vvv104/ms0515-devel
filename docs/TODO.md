# TODO

Work the programs here owe to what the machine turned out to be.  The
emulator (`../ms0515`) is built from the findings of the FPGA rebuild
of the board (ms0515-fpga), and since its v1.21.0 (2026-11-07) it
keeps two of the machine's habits the programs here were written
without: the diskette turns, and the keyboard's auto-repeat is the
firmware's metronome code.  (The frame rate stays 50 Hz: the kits'
`CLOCK = 50.` was right - the timer interrupt is the frame sync, and the
machines kept time in banks.)  Each item says what
the machine does, what the program does, and what it takes.  Nothing
here is a defect of the emulator: the core follows the machine, and the
programs follow the core.

## MANICM's held keys expire under the metronome code

**The machine.**  The MS 7004 sends a key's code when it goes down and
nothing when it comes up; while a key stays down it sends 264 (the
LK201's metronome) at its repeat rate - never the key's code again
(the firmware's `L_2B0`; the emulator's `docs/hardware/keyboard.md`,
264).  Both ROMs understand 264 and repeat the last key themselves; a
program that reads the USART itself sees the code once and 264 after.

**The program.**  `projects/manicm/MS0515.MAC`, KEYS: a key is held for
KTMR passes after its code and KTMRR after each repeat, and a repeat is
recognised by *the key's own code* coming again (`CMPB R1,R0` against
the key table).  With 264 in place of it no timer is refreshed: an
arrow held down expires after KTMR passes and Willy stops walking.  The
comment above KEYS describes the old core's keyboard, which resent the
key.

**What it takes.**  Keep the code of the newest key down; on 264
refresh that key's timer as its own code would (and, as FIST does,
any other timer still running - keys pressed together stay held while
the newest repeats).  `tests/ManicmGame.hpp` runs the emulator's
keyboard with its auto-repeat, so the test that holds a key across the
repeat delay shows it.  Twenty lines and the comment.

## FIST's KSCAN names the repeat code 254

**The program.**  `projects/fist/source/game_keys.py`, KSCAN: every
code refreshes the running timers first (KREFR, the chord rule), so a
264 keeps the held keys held and the game plays; but the line after it,
`CMP R0,#254 ; auto-repeat code (real MS7004)`, names the technical
description's figure, which the keyboard never sends (the ROM maps 254
to `$`), and 264 falls through to the key tables and matches nothing.

**What it takes.**  264 in that compare and in the note; a look at
`KSCAN_NOTE`, which says "a key's make / repeat code sets its timer" -
the repeat code sets no timer of its own, the refresh does the work.

## Saper's held keys do not repeat

**The program.**  `projects/saper/source/gen_saper.py` polls the USART
(`LPK`, `LPKD`) and acts on the code's value - F2, the arrows, the
digits.  A key held down used to repeat its action on the old core's
repeated code; now the repeats are 264, which matches nothing, so a
held arrow moves the cursor once.

**What it takes.**  Decide whether a held arrow should walk: if so,
remember the last code and take 264 as that code again; if not, say so
in the README, which describes the polling.

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

## Not affected, looked at

- **UBIX** takes its keys through the ROM (`kernel/tty.c`, `rom.s`),
  which understands 264: auto-repeat works as before.  Its tty is the
  ROM's.
- **STARS, Minesweeper, DECUS C, SL** read no USART of their own.
- **The keyboard's clock** (the timer's counter 0, which the emulator
  now listens to): the ROM programs it at boot and none of the programs
  here touches counters 0 and 1.
