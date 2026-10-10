# CRYPT

A dungeon crawl in the first person for the MS 0515, in C on the GCC
toolset (`rt11_devel/toolset/gcc`): a party of four walks the crypt a
cell at a time, as in Eye of the Beholder, but every step and turn is
shown in parts, the walls sliding and swinging, since the view is cast
by rays rather than drawn from pictures.  This is the beginning: the
view, the screen round it, a test level.  The party, the monsters, the
turns by speed and the levels from disk come next.

| key | what |
|---|---|
| Up, Down | a step forward, back |
| Left, Right | a turn |
| Z, X | a step to the left, to the right |
| joystick | the same four |
| T | the walls with their bricks, or flat |
| Q | back to the monitor |

## Building and playing

```
cmake -S rt11_devel/projects/crypt -B rt11_devel/projects/crypt/build -G Ninja
cmake --build rt11_devel/projects/crypt/build
cd rt11_devel/projects/crypt/build/sav && ms0515-run CRYPT
```

The compiler is `$MS0515_GCC/bin/pdp11-aout-gcc`, or on the PATH; the
build needs Python with Pillow for the art.  On Windows the build runs
in WSL and `ms0515-run.exe` from there.

## The screen

```
 0        10        20        30       39
 +--------------------------------------+ 0
 | the place, the day, the last message | 0-15    two lines of text
 +----------------------+---------------+ 16
 |                      | hero 1 | hero 2|
 |   the view, 22 cells | face   | face  | 16-75   faces of 32x32, names,
 |   by 120 rows        +--------+-------+ 76      bars of life and mana
 |                      | hero 3 | hero 4|
 |                      |        |       | 76-135
 +------+---------------+--------+-------+ 136
 | turn | the map and   | attack | spell |
 | step | the compass   | wait   |       | 136-199
 +------+---------------+----------------+
```

The view is 176 by 120 pixels, Eye of the Beholder's; the buttons are
drawn for a screen tapped at - the page that shows the machine will
turn a tap into the key - and the keys do the same meanwhile.

## How it works

`src/view.c` casts a ray a cell column, 22 of them, from the party's
fractional position in the direction the column looks (Lode
Vandevenne's way, in 8.8 fixed point): the ray walks the grid an edge
at a time and the wall it meets stands as tall as its distance allows,
from a table, with no division anywhere.  A wall is drawn flat, its
shade by distance and by the side it shows, or with the brick texture
(`tex.h`), the column by where the ray met the cell; the ceiling and
the floor are painted only where the wall stood last time and stands
no more.  `src/map.c` is the test level, drawn as text; `src/hud.c`
the screen round the view; `src/main.c` the loop - a pass every two
frames, a step or a turn in eight passes, the keys held as the
keyboard's codes come.

The art is PNGs under `art/`, turned into C by `tools/mkart.py` at
build time; what is there now is drawn by `art/placeholders.py` and
stands in until real pictures come.

What the machine does: flat walls turn round at 25 passes a second;
the bricks at about 8, until their loop is assembler too (the
column fill already is: `ms_vfill`).  On its way out the program
prints how many passes it made in how many frames.

## Tests

`crypt_tests` (`tests/`), built with the emulator's tests, runs
`build/sav/CRYPT.SAV` on ms0515-run's machine: the screen with its
walls, band, party and buttons; a step and a turn shown in parts; a
wall that will not be walked through; the pace held turning round and
round.  `--crypt-sav=<folder>` names another build, and then a missing
program is a failure (CI).
