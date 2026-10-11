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
across it, where each stands at this phase, to four cells - farther
the rows crowd into a block; the ceiling is black.  A cell row of the
screen has two colours, and where a wall's slanting edge crosses a
cell on its way down to the floor the row is part wall and part
floor: there the wall's red is the paper and the floor's green the
ink, and the black of the lines waits for a row that is wall
throughout - else the floor came out black in triangles under the
wall's foot.  Where a wall ends - a wall across the axis at an edge
whose neighbour shows no face across, a wall along it at an end with
floor before or past it - a line stands down the corner, and no joint
comes near it.  A wall across the axis has its top and its foot lined
too, where it meets the ceiling and the floor; a wall along the axis
meets them on a slant that crosses the cells, and a cell row part wall
and part floor has no third colour for a line, so there the edge is
where the red meets the green.  One threshold keeps the detail from
crowding into black far away: no two lines nearer than four pixels,
the first of a crowd kept and the rest left out, so that something is
always drawn.  A wall has its rows of mortar when its brick rows are
four rows tall; shorter, it keeps the horizon's line, its top and foot
(across the axis), its corners and a joint a brick, the wall's height,
down to four rows tall, under which it is a fill.  Joints nearer than
four pixels along a wall are left out, and the floor's lines along
the axis nearer than that to one another, from the farthest plane's
on.

Each pass `src/view.c` finds what each column shows - the cells about
the party gone through front to back, each wall face taking the pixel
columns not yet taken, a wall along the axis as one face however long
- and keeps in memory a layer of the view's columns composed: the
walls along the axis with their outline and mortar, the walls across
it with theirs, the floor and its lines along, the ceiling.  A column
of the layer is composed again only when the walls in it change, and
when a wall across the axis grows as the party comes, or shrinks as it
goes back, only the rows it takes or leaves are done again.  Then each
column goes up whole: what moves - the joints of the bricks where they
fall at this phase, the floor's edges across - is drawn into the
column, the column is put on the screen in one go, and the drawing is
taken out of it again, so that nothing is ever on the screen without
the rest.  The loops of a row of the column are `src/rows.s`; the
finding of the joints and the corners, and the column's going up, are
`src/joints.s` - assembler, since the compiler made three times the
code of them and ran it slower.  The tables are `geometry.c`, made
with the header, so that the assembler reads them too.
`src/map.c` is the test level, drawn as text and read as it is;
`src/hud.c` the screen round the view, with the map of the cells about
the party, the party's arrow in the middle and the level sliding under
it; `src/main.c` the loop: a step in five passes, a turn in eight, a
pass of either in five frames.

The faces are PNGs under `art/`, turned into C by `tools/mkart.py` at
build time; what is there now is drawn by `art/placeholders.py` and
stands in until real pictures come.

What the machine does: a pass of a turn in 3.5 frames, a pass of a
step in 4.7 - ten a second, half a second a step, four fifths a
quarter turn.  On its way out the program prints how many passes it
made in how many frames, and how many of them drew the view in how
many; a build with `-DPROF=1` finds the pieces and paints nothing,
`-DPROF=2` paints without finding, `-DPROF=5` leaves out the joints,
`-DPROF=6` the layer's keeping up, for measuring the parts; `-DPACE=n`
sets one pace for everything.  A pass of a step goes, by those: the
joints found, drawn and taken out again about a frame, the finding of
the pieces and the columns composed again (two a pass, at the far end
of the side walls) about one each, the columns' going up half a one,
the rest bookkeeping.  The finding of the pieces is the next thing
for assembler, for speed and for room both.  The program is 62
blocks, 16.1 KB of code and 14.1 KB of data, and ends under the video
memory's window at 0100000 with a kilobyte and a half to spare: the
level's text is read as it is, the pieces keep a plane's and a wall's
fields over each other, the stack is 512 bytes (a run through every
move uses under 200), the band's title is put together without the
runtime's string.c, and the screen round the view, the level and the
loop are compiled for size (the view is not: the compiler's size mode
calls subroutines for shifts, and a pass slows by a third).  Any more
- other kinds of wall, floor and ceiling, monsters - needs the memory
banks: the code and the stack under 040000, the data from there in
the primary bank with the extended banks behind it for what is not
in use every pass; the code is a few hundred bytes from fitting.

A trap of the assembler, beside the toolset README's: a conditional
branch reaches 254 bytes and `as` says nothing when the target is
farther - the displacement wraps and the branch lands in the middle
of another instruction, which runs on as garbage until it traps far
away (`jmp` is the long form).  Numbers without a leading 0 are
decimal there, offsets included.

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
edges; the wall's foot meeting the floor with no black under it; a
step that runs the joints and moves no wall; two steps on and two
back leaving the picture as it was composed whole; a turn that slides
the picture four columns a pass, comes back exactly and comes round
the four quarters exactly; a wall that will not be walked through;
the paces held, with what a pass costs told.
`--crypt-sav=<folder>` names another build, and then a missing program
is a failure (CI); `--crypt-png=<folder>` keeps what the tests saw as
PNGs.
