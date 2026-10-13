# CRYPT

A dungeon crawl in the first person for the MS 0515, in C on the GCC
toolset (`rt11_devel/toolset/gcc`): a party of four walks the crypt a
cell at a time, as in Eye of the Beholder, but every step and turn is
shown in parts, the walls running past and swinging round.  This is
the beginning, begun a second time from the geometry alone: the view
as the dungeon's edges, the screen round it, a test level.  The fill
of the walls, the floor and the ceiling, the party, the monsters, the
turns by speed and the levels from disk come next.

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
60 / r rows above and below the horizon at r cells along the column's
way.  Two things follow.  When the party steps along a corridor the
walls beside it keep their angle, every pixel column its height; only
the seams between the cells run along them.  And a turn is the sector
sliding past: the picture moved four columns a pass and the four
coming in drawn, eight passes a quarter turn, nothing redrawn.  The
price is that horizontal lines bow a little towards the window's
edges, as they do in a lens.

The view is the dungeon's edges, in one colour on one: every seam of
the world's squares, to the pixel.  A wall along the axis has its top
and its foot a pixel in each column, a line on a slant; the seams
between its cells, and its ends, stand down it where the corners
fall.  A wall across the axis is a frame, standing on the floor's edge
before it: its foot is the row above that edge, and the two are one
line.  The floor's cells have their edges across the axis, a row in
each column, and along it, a slant like a wall's foot; the ceiling the
same upside down; an edge right under a wall's foot is the foot.  What
a wall hides is not drawn, but where a wall ends with another farther
behind it, the corner is the nearer one's, a line down its last
pixel.  Nearer than four pixels to a line drawn no other seam between
cells is: far away the cells crowd, and only the first of a crowd
shows; a face's own edge, the shape of the place, gives way only to a
line beside it.  The machine's cells of eight pixels have two colours,
and that is why the view is one colour with black lines: a fill that
met another on a slant would have to share a cell's row with the line
between them, a third colour, and the line would have to go, or step
by the cell.  How the fill comes back is the next question.

`art/geometry.py` makes the tables: for every pixel column of the
sector, the height of a wall half a cell, one and a half, two and a
half and three and a half from the party's axis; where every cell
corner falls at every phase of a step; where the edges of the floor's
cells cross each column; the sine, cosine and tangent of each column.
Each pass `src/view.c` finds what each pixel column shows - the cells
about the party gone through front to back, each wall face taking the
pixel columns not yet taken, a wall along the axis as one face however
long - and draws each cell column of the view from its pieces into a
column in memory, which goes up whole.  `src/map.c` is the test level,
drawn as text and read as it is; `src/hud.c` the screen round the
view, with the map of the cells about the party, the party's arrow in
the middle and the level sliding under it; `src/main.c` the loop: a
step in five passes, a turn in eight, a pass of either in five
frames.

The faces are PNGs under `art/`, turned into C by `tools/mkart.py` at
build time; what is there now is drawn by `art/placeholders.py` and
stands in until real pictures come.

What the machine does: a pass of a turn in 1.5 frames, a pass of a
step in 4.7 - ten a second, half a second a step, four fifths a
quarter turn.  On its way out the program prints how many passes it
made in how many frames, and how many of them drew the view in how
many; `-DPACE=n` sets one pace for everything, and `-DPROF=1` finds
the pieces and draws nothing, `-DPROF=2` draws but puts nothing on the
screen, `-DPROF=3` clears and puts the columns without edges,
`-DPROF=4` draws the walls' edges alone, for measuring the parts.  A
pass of a step goes, by those: the finding of the pieces 0.9 frames,
the columns cleared 0.8, put on the screen 0.7, the walls' edges 1.9,
the floor's and the ceiling's 0.4.  The program is 38 blocks.  The project is C throughout; what the machine library
does in assembler (`ms_vput`, `ms_hscroll`) is the toolset's, with
its tests.

Not done yet: the fill of the walls, the floor and the ceiling; steps
to the side shown in parts (they are instant); other kinds of wall;
and monsters.

## Tests

`crypt_tests` (`tests/`), built with the emulator's tests, runs
`build/sav/CRYPT.SAV` on ms0515-run's machine: the screen with its
edges, band, party and buttons; the left wall's top and foot straight
to the horizon, pixel by pixel, against the cylinder's own sum; the
seams where the cells' corners fall, the floor's edge across right up
to the wall's foot and not into the wall, and the ceiling's its
mirror; the wall ahead's foot one line with the floor's edge under
it; a wall ending before one farther behind, its corner a line; a step
that runs the seams and moves no edge; two steps on
and two back, and a turn there and back, leaving the picture as it
was; the wall ahead as a frame with its seams; a turn that slides the
picture four columns a pass, comes back exactly and comes round the
four quarters exactly; a wall that will not be walked through; the
paces held, with what a pass costs told.  `--crypt-sav=<folder>`
names another build, and then a missing program is a failure (CI);
`--crypt-png=<folder>` keeps what the tests saw as PNGs.
