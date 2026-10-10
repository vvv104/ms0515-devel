/* CRYPT - a dungeon crawl in the first person, for the MS 0515.
 *
 * The party stands on a cell of the dungeon facing one of the four
 * ways, as in Eye of the Beholder; a step takes it to the next cell
 * and a turn a quarter round, but shown in parts, pass by pass, so the
 * walls run past and swing round instead of jumping.  The keys: Up and
 * Down step forward and back, Left and Right turn, Z and X step
 * sideways, the joystick the same; Q ends it, and on the way out the
 * program says how many passes it made in how many frames.
 *
 * The view is view.c, the rest of the screen hud.c, the dungeon map.c,
 * the walls drawn on the host data.c; the machine is the toolset's
 * ms0515.h. */

#include <ms0515.h>
#include <rt11.h>
#include "art.h"
#include "data.h"
#include "hud.h"
#include "map.h"
#include "planes.h"
#include "view.h"

/* Frames a pass takes, by what the pass has to draw: a turn slides
 * the picture and draws four columns, a step draws all twenty-two. */
#ifndef PACE
#define TURN_PACE 3			/* 16 a second: a quarter turn in half a second */
#define STEP_PACE 4			/* 12 a second: a step in two fifths */
#define IDLE_PACE 2			/* standing: the keys looked at */
#else
#define TURN_PACE PACE			/* one pace for all, for measuring */
#define STEP_PACE PACE
#define IDLE_PACE PACE
#endif

enum { kForward, kBack, kLeft, kRight, kSideLeft, kSideRight, kKeys };

static struct ms_held keys[kKeys] = {
	{MS_KEY_UP, 0}, {MS_KEY_DOWN, 0}, {MS_KEY_LEFT, 0}, {MS_KEY_RIGHT, 0},
	{MS_KEY_Z, 0}, {MS_KEY_X, 0},
};
static const struct hero heroes[4] = {
	{"ARNO", &face1, 42, 42, 0, 0}, {"OTTO", &face2, 32, 32, 5, 10},
	{"ELSA", &face3, 18, 18, 20, 20}, {"VERA", &face4, 30, 30, 12, 12},
};
static struct view view;
static int stepping;			/* +1 forward, -1 back: the phase moves that way */
static int turning;			/* the pass of a turn, 1..TURN_PASSES, 0 for none */
static int turnDir;
static int moved;			/* the party on another cell at once: the view to draw */
static int pace = IDLE_PACE;		/* frames the pass in hand takes */
static unsigned passes;
static unsigned drawn, drawnFrames;	/* the passes that drew the view, and the frames they took */

static void draw(void)
{
	unsigned from = ms_frames;

	view_render(&view);
	drawn++;
	drawnFrames += ms_frames - from;
}

static void slide(int pass)
{
	unsigned from = ms_frames;

	view_turn_pass(pass);
	drawn++;
	drawnFrames += ms_frames - from;
}

/* The way the party faces, as a unit step in cells: 0 east, 1 south. */
static void facing(int f, int *ux, int *uy)
{
	static const signed char dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};

	*ux = dx[f & 3];
	*uy = dy[f & 3];
}

static void mapped(void)
{
	hud_map(view.cellX, view.cellY, view.facing << 6);
}

/* A step forward or back begun, if the cell there is floor: forward
 * the phases run up from the party's cell, back they run down from
 * the cell behind, which the party is then on. */
static void step(int dir)
{
	int ux, uy, tx, ty;

	facing(view.facing, &ux, &uy);
	tx = view.cellX + dir * ux;
	ty = view.cellY + dir * uy;
	if (map_at(tx, ty))
		return;
	stepping = dir;
	pace = STEP_PACE;
	if (dir > 0) {
		view.phase = 1;
	} else {
		view.cellX = tx;
		view.cellY = ty;
		view.phase = PLANE_PHASES - 1;
		mapped();
	}
}

/* A step to the side, at once. */
static void sidestep(int dir)
{
	int ux, uy, tx, ty;

	facing(view.facing + dir, &ux, &uy);
	tx = view.cellX + ux;
	ty = view.cellY + uy;
	if (map_at(tx, ty))
		return;
	view.cellX = tx;
	view.cellY = ty;
	moved = 1;
	mapped();
}

static void turn(int dir)
{
	turning = 1;
	turnDir = dir;
	pace = TURN_PACE;
	view_turn_begin(&view, dir);
}

/* What the keys and the joystick ask, once the last move is over. */
static void command(void)
{
	unsigned joy = ms_joystick();

	if (ms_is_held(keys[kForward]) || (joy & MS_JOY_UP))
		step(1);
	else if (ms_is_held(keys[kBack]) || (joy & MS_JOY_DOWN))
		step(-1);
	else if (ms_is_held(keys[kLeft]) || (joy & MS_JOY_LEFT))
		turn(-1);
	else if (ms_is_held(keys[kRight]) || (joy & MS_JOY_RIGHT))
		turn(1);
	else if (ms_is_held(keys[kSideLeft]))
		sidestep(-1);
	else if (ms_is_held(keys[kSideRight]))
		sidestep(1);
}

/* A step's phase advanced: the last phase forward lands on the next
 * cell, the last one back on the cell's centre. */
static void stepOn(void)
{
	int ux, uy;

	if (stepping > 0) {
		if (++view.phase == PLANE_PHASES) {
			facing(view.facing, &ux, &uy);
			view.cellX += ux;
			view.cellY += uy;
			view.phase = 0;
			stepping = 0;
			mapped();
		}
	} else {
		if (--view.phase == 0)
			stepping = 0;
	}
	if (!stepping)
		pace = IDLE_PACE;
}

/* A pass: the keys read, the move advanced or a new one begun, the view
 * drawn.  0 on Q. */
static int pass(void)
{
	int c;

	for (c = 0; c < pace; c++)		/* the hold timers count frames, a pass is `pace` of them */
		ms_held_tick(keys, kKeys);
	while ((c = ms_key()) >= 0) {
		if (c == MS_KEY_Q)
			return 0;
		ms_held_code(keys, kKeys, c);
	}
	if (turning) {
		slide(turning);
		if (++turning > TURN_PASSES) {
			turning = 0;
			pace = IDLE_PACE;
			view.facing = (view.facing + turnDir) & 3;
			mapped();
		}
		return 1;
	}
	if (stepping) {
		stepOn();
		draw();
		return 1;
	}
	command();
	if (turning)				/* begun just now: its first pass */
		slide(turning++);
	else if (stepping || moved)		/* begun just now, or a step aside */
		draw();
	moved = 0;
	return 1;
}

/* A number in decimal on the console: printf is a fifth of the room
 * the program has. */
static void sayNumber(unsigned n)
{
	char digits[6];
	int i = 6;

	digits[--i] = '\200';			/* the string's end without a new line */
	do {
		digits[--i] = (char)('0' + n % 10);
		n /= 10;
	} while (n && i > 0);
	rt11_print(digits + i);
}

int main(void)
{
	unsigned last, started;
	int i;

	map_test();
	if (data_load() < 0) {
		rt11_print("CRYPT.DAT is missing");
		return 1;
	}
	view.cellX = map_startX;
	view.cellY = map_startY;
	view.facing = 0;
	view.phase = 0;
	ms_screen_begin(0);
	ms_clock_begin();
	ms_keys_begin();
	hud_frame();
	hud_title("THE CRYPT", 1);
	hud_message("THE AIR IS COLD AND STILL.");
	for (i = 0; i < 4; i++)
		hud_hero(i, &heroes[i]);
	mapped();
	view_render(&view);
	started = last = ms_frames;
	do {
		while (ms_frames - last < (unsigned)pace)
			;
		last = ms_frames;
		passes++;
	} while (pass());
	ms_keys_end();
	ms_clock_end();
	ms_screen_end();
	/* How the machine kept up: the frames of the loop alone, the data's
	 * loading and the screen's making before it left out; then the
	 * passes that drew the view and the frames they took, to a frame. */
	rt11_print("CRYPT: \200");
	sayNumber(passes);
	rt11_print(" passes in \200");
	sayNumber(ms_frames - started);
	rt11_print(" frames, \200");
	sayNumber(drawn);
	rt11_print(" drawn in \200");
	sayNumber(drawnFrames);
	rt11_print("");
	return 0;
}
