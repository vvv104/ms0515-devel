# CELWAR - CellWar for the MS-0515 / RT-11

A war of cells on a board of 20 x 10, in the fog, the human against the
machine.  The game is the owner's own design (the rules below are its
CellWar, with one change to the factory made for this version); the
program is written for the MS-0515 in MACRO-11, the game in one file and
the machine in another, like MANICM.

## Files

```
rt11_devel/projects/cellwar/
├── README.md        this file
├── CELWAR.MAC       the game: the rules, the turn, the computer's play,
│                    the screen, the saved game
├── MS0515.MAC       the machine: cells and text over the VRAM window, the
│                    keyboard by interrupt, the frame clock, a click
├── build.toml       the build: three objects, linked with a map
├── source/
│   └── gen_art.py   pre_build: ART.MAC, the six tiles and the font, drawn
│                    as text
└── tests/           the harness: the game booted through the emulator library
```

Build artifacts, never committed: `CELWAR.SAV` (13 blocks), `CELWAR.MAP`
(the link map, which names the state the tests read), `ART.MAC`.

```
python rt11_devel/toolset/build.py rt11_devel/projects/cellwar/build.toml
```

RT-11 names are six letters, so the program is `CELWAR`.

## The rules

- **The board** is 20 x 10 cells, no obstacles.  Two players, the human
  and the computer, in turn; the human first.
- **A unit has four stages**, one counter that standing raises and moving
  spends: the infantry, fortification 1, fortification 2, the factory.
- **Every unit makes exactly one move a round**, a step (dx, dy) of -1..1
  each.  The player moves all their combat units (the first three
  stages) first, then their factories.  A unit made this round has moved
  already.
- **Standing** (0,0) raises the stage by one.  A factory that stands
  stays a factory.
- **A combat unit that steps** becomes an infantry and goes: onto an empty
  cell it moves; onto an enemy infantry or factory it kills it and takes
  the cell; onto an enemy fortification it dies and the fortification
  becomes an infantry - a second attacker the same round kills that; onto
  its own unit, or off the map, it cannot go.
- **A factory that steps** onto an empty cell makes a new infantry there
  and drops to fortification 1; it cannot step anywhere else.  So it
  builds once in three rounds at most, and a player builds when they
  choose.
- **The fog**: a player sees the cells within 1 of each of their units,
  within 2 of a fortification.  What was in view at the start of the turn
  stays in view until the turn ends.  The computer sees the same way.
- **The end**: the last player with a unit wins.
- **The start**: each side has one fortification 2, the human's at (4,4),
  the computer's at (15,5).

## Playing

`CELWAR.SAV` on an RT-11 volume, `R CELWAR` (`package/assets/disks/cellwar.dsk`
boots straight into it: `ms0515-cli --disk0 cellwar.dsk`, then `R CELWAR`).

| key | what |
|---|---|
| TAB | the next unit that has a move to make: the combat units round and round, then the factories |
| arrows | the cursor a step from the selected unit, no further |
| SPACE | the move: the selected unit steps to the cursor's cell, or stands when the cursor is on it |
| S | save the game to `DK:CELWAR.GAM` (one slot, mid-turn included) |
| L | load it |
| N | a new game, once this one is over |
| Q | leave for RT-11 |

The selected unit sits on blue; the cursor's cell flashes; a unit that
has moved this round is dim.  The human's units are green, the
computer's red.  A refused move clicks and says so.  When the last unit
has moved the computer plays, its moves in the human's view shown as they
go, and the next round begins.

## The computer's play

`CELWAR.MAC`, "the computer's play": for each of its units in turn the
nine steps are scored and the best taken, with a little chance thrown in.
It sees only what its units see (`VISC`).  The scores are named
constants at the top of the section, made to be tuned: a kill (a factory
above an infantry), a hit on a fortification only with a partner next to
it to finish the job, a step next to an enemy combat unit costs (it
strikes first), a step nearer the goal gains - the goal being the nearest
enemy in view, else the one last seen, else the human's corner - a step
next to an enemy factory gains (the kill comes next), a step next to an
own factory costs (its room to build); standing gains a little by stage,
more with one enemy next door (the fortification takes the hit), more
with no enemy in view (the economy), but a fortification 2 does not stand
into a factory with an enemy within three cells; and the army goes
forward the larger it grows.  A factory always builds, toward the goal.

## The machine

`MS0515.MAC` documents its interface at the top: take the machine, draw a
cell, print a string, read a key, click, wait.  The board is 20 x 10
cells of 16 x 16 pixels in the 320 x 200 colour mode, the five text rows
under it; a VRAM word is the attribute byte over eight pixels, so a cell
is 16 rows of two words and a glyph 8 words down a column.  The window at
100000..137777 is open for each draw only; the keyboard and frame
interrupts are served below it.  The dispatcher reads back, and the game
restores it as found rather than assuming a value: started by STARTS.COM
on the toolset's folder device it found 003177, the frame interrupt on.
The keyboard sends no key-up codes; the game needs none, a key is a
press.

## Tests

`tests/` boots the built game through the emulator library, pokes a
position onto the board where the link map says it is, presses the keys
through the library and reads the board back: the stages, the moves, the
attacks, the factory, the turn order, the fog, the end of the game, the
exit; the computer's kills, its restraint before a lone fortification,
the pair that takes one, its factory that builds; the game saved and
loaded.  The suite skips itself when the game is not built.
