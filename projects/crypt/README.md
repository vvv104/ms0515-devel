# CRYPT

A dungeon crawl in the first person for the MS 0515, in C on the GCC
toolset (`rt11_devel/toolset/gcc`): a party of four walks the crypt a
cell at a time, as in Eye of the Beholder, but every step and turn is
shown in parts, the walls running past and swinging round.  This is
the beginning: the view, the screen round it, a test level.  The
party, the monsters, the turns by speed and the levels from disk come
next.

| key | what |
|---|---|
| Up, Down | a step forward, back |
| Left, Right | a turn |
| Z, X | a step to the left, to the right (at once, for now) |
| joystick | the same four |
| Q | back to the monitor |

## Building and playing

```
cmake -S rt11_devel/projects/crypt -B rt11_devel/projects/crypt/build -G Ninja
cmake --build rt11_devel/projects/crypt/build
cd rt11_devel/projects/crypt/build/sav && ms0515-run CRYPT
```

The compiler is `$MS0515_GCC/bin/pdp11-aout-gcc`, or on the PATH; the
build needs Python with Pillow for the art.  On Windows the build runs
in WSL and `ms0515-run.exe` from there.  `CRYPT.DAT` comes out beside
the program and must stay beside it: the walls, drawn on the host.

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
 | turn | the map, the  | attack | spell |
 | step | party mid-way | wait   |       | 136-199
 +------+---------------+----------------+
```

The view is 176 by 120 pixels, Eye of the Beholder's; the buttons are
drawn for a screen tapped at - the page that shows the machine will
turn a tap into the key - and the keys do the same meanwhile.

## How it works

The view is a cylinder round the party, not a flat picture: a pixel
column looks one way for all its rows, a quarter of an angle unit
(256 to the turn) from the next, so 32 cell columns make a quarter
turn and the 22 of the view are the middle of them.  Two things
follow.  When the party steps along a corridor the walls beside it
keep their angle - every pixel column keeps its height and its rows
keep their texture row - and only the texture runs along them; so
`art/planes.py` draws those walls on the host, pixel by pixel from the
brick PNG, for each of the five phases of a step, and the machine
copies their columns (`CRYPT.DAT`, loaded at the start into the bank
above the program, both halves of it; `src/data.c`).  And a turn is
the sector sliding past: the picture moved four columns a pass and the
four coming in drawn, eight passes a quarter turn, nothing redrawn.
The price is that horizontal lines bow a little towards the window's
edges, as they do in a lens.

`src/view.c` finds what each column shows: the cells about the party
gone through front to back, each wall face taking the pixel columns
not yet taken from a table of where every cell corner falls, a wall
along the axis as one face however long; then it paints the columns,
farthest piece first, the nearer ones over it under their pixel masks.
A wall across the axis is drawn by texture columns (`tex.h`), a
reduced texture for the distance.  The right-hand walls are the
left-hand images mirrored, pixel by pixel, which halves the data.
`src/map.c` is the test level, drawn as text; `src/hud.c` the screen
round the view, with the map of the cells about the party, the
party's arrow in the middle and the level sliding under it;
`src/main.c` the loop: a step in five passes of four frames, a turn
in eight of three.

The art is PNGs under `art/`, turned into C by `tools/mkart.py` at
build time; what is there now is drawn by `art/placeholders.py` and
stands in until real pictures come.  The program must end under
060000 (the build checks: `LIMIT`), since the bank above it holds the
walls.

What the machine does: a pass of a turn in 1.3 frames, a pass of a
step in 3.2 - the paces leave a little room.  On its way out the
program prints how many passes it made in how many frames, and how
many of them drew the view in how many; a build with `-DPROF=1` finds
the pieces and paints nothing, `-DPROF=2` paints without finding, for
measuring the two apart.

Not done yet: the floor's cell edges (lines in the first version, too
dear a multiply at a time; a table will bring them back), steps to the
side shown in parts, the planes farther than two and a half cells
(a hall's far walls come out as a bar on the horizon).

## Tests

`crypt_tests` (`tests/`), built with the emulator's tests, runs
`build/sav/CRYPT.SAV` on ms0515-run's machine: the screen with its
walls, band, party and buttons; the left wall's top edge straight to
the horizon, pixel by pixel, against the cylinder's own sum; a step
that runs the texture and moves no wall; a turn that slides the
picture four columns a pass, comes back exactly and comes round the
four quarters exactly; a wall that will not be walked through; the
paces held, with what a pass costs told.  `--crypt-sav=<folder>` names
another build, and then a missing program is a failure (CI);
`--crypt-png=<folder>` keeps what the tests saw as PNGs.
