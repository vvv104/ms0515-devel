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
build needs Python with Pillow for the faces.  On Windows the build
runs in WSL and `ms0515-run.exe` from there.

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
turn and the 22 of the view are the middle of them.  A wall stands
60 / r rows above and below the horizon at r cells.  Two things
follow.  When the party steps along a corridor the walls beside it
keep their angle, every pixel column its height; only the joints of
the bricks run along them.  And a turn is the sector sliding past:
the picture moved four columns a pass and the four coming in drawn,
eight passes a quarter turn, nothing redrawn.  The price is that
horizontal lines bow a little towards the window's edges, as they do
in a lens.

The walls are not pictures.  `art/geometry.py` makes the tables: for
every pixel column of the sector, the height and the depth of the
wall half a cell, one and a half and two and a half from the party's
axis; where every cell corner falls at every phase of a step; where
the edges of the floor's cells cross it.  From these `src/view.c`
draws a brick wall as lines on a fill: its outline, three rows of
mortar at the quarters of its height, and the joints between the
bricks - half a cell long, a quarter tall, the rows laid by turns -
where each falls at this phase.  The floor is green with the edges of
its cells drawn over: along the axis, at every plane's foot, and
across it, where each stands at this phase; the ceiling is black.

Each pass `src/view.c` finds what each column shows - the cells about
the party gone through front to back, each wall face taking the pixel
columns not yet taken, a wall along the axis as one face however long
- and keeps in memory a layer of what does not move in a step: the
walls along the axis, their outline and mortar, the floor and its
lines along.  A column of the layer is composed again only when the
walls in it change.  The layer is copied to the screen and what moves
is drawn over it: the joints, the walls across the axis (which grow as
the party comes), the floor's edges across.  `src/map.c` is the test
level, drawn as text; `src/hud.c` the screen round the view, with the
map of the cells about the party, the party's arrow in the middle and
the level sliding under it; `src/main.c` the loop: a step in five
passes, a turn in eight, a pass of either in five frames.

The faces are PNGs under `art/`, turned into C by `tools/mkart.py` at
build time; what is there now is drawn by `art/placeholders.py` and
stands in until real pictures come.

What the machine does: a pass of a turn in 4.0 frames, a pass of a
step in 4.5 - ten a second, half a second a step, four fifths a
quarter turn.  On its way out the program prints how many passes it
made in how many frames, and how many of them drew the view in how
many; a build with `-DPROF=1` finds the pieces and paints nothing,
`-DPROF=2` paints without finding, `-DPROF=3` leaves out what moves,
`-DPROF=4` the layer's copy, for measuring the parts; `-DPACE=n` sets
one pace for everything.  The time goes, in C, to the joints (a
pixel a row), the layer's copy, the finding and the floor's edges;
the loops of those are the next thing to write in assembler.

Not done yet: steps to the side shown in parts (they are instant);
the planes farther than two and a half cells (a hall's far walls come
out as a bar on the horizon); other kinds of wall than brick; and
monsters, which will be sprites (`tex.h`) over the layer.

## Tests

`crypt_tests` (`tests/`), built with the emulator's tests, runs
`build/sav/CRYPT.SAV` on ms0515-run's machine: the screen with its
walls, band, party and buttons; the left wall's top edge straight to
the horizon, pixel by pixel, against the cylinder's own sum; the rows
of mortar at the quarters of the wall, the joints between, the floor's
edges; a step that runs the joints and moves no wall; a turn that
slides the picture four columns a pass, comes back exactly and comes
round the four quarters exactly; a wall that will not be walked
through; the paces held, with what a pass costs told.
`--crypt-sav=<folder>` names another build, and then a missing program
is a failure (CI); `--crypt-png=<folder>` keeps what the tests saw as
PNGs.
