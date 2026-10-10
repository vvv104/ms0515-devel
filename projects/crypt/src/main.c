/* CRYPT - a dungeon crawl in the first person, for the MS 0515.
 *
 * The party stands on a cell of the dungeon facing one of the four
 * ways, as in Eye of the Beholder; a step takes it to the next cell
 * and a turn a quarter round, but shown in parts, pass by pass, so the
 * walls slide and swing instead of jumping.  The keys: Up and Down step
 * forward and back, Left and Right turn, Z and X step sideways, the
 * joystick the same; T draws the walls with their texture or flat; Q
 * ends it, and on the way out the program says how many passes it made
 * in how many frames.
 *
 * The view is view.c, the rest of the screen hud.c, the dungeon map.c;
 * the machine is the toolset's ms0515.h. */

#include <ms0515.h>
#include <stdio.h>
#include "art.h"
#include "hud.h"
#include "map.h"
#include "view.h"

#ifndef PACE
#define PACE 2				/* frames a pass takes: 25 a second */
#endif
#define MOVE_PASSES 8			/* passes a step or a turn is shown in */

enum { kForward, kBack, kLeft, kRight, kSideLeft, kSideRight, kKeys };

static struct ms_held keys[kKeys] = {
	{MS_KEY_UP, 0}, {MS_KEY_DOWN, 0}, {MS_KEY_LEFT, 0}, {MS_KEY_RIGHT, 0},
	{MS_KEY_Z, 0}, {MS_KEY_X, 0},
};
static const struct hero heroes[4] = {
	{"ARNO", &face1, 42, 42, 0, 0}, {"OTTO", &face2, 32, 32, 5, 10},
	{"ELSA", &face3, 18, 18, 20, 20}, {"VERA", &face4, 30, 30, 12, 12},
};
#ifndef TEXTURED
#define TEXTURED 0			/* the walls flat to begin with; T switches */
#endif
static struct view view = {0, 0, 0, TEXTURED};
static int left, dx, dy, dAngle;	/* the move in progress: passes left and a pass's share */
static unsigned passes;

/* The way the party faces, as a unit step in cells: 0 east, 64 south. */
static void facing(int angle, int *ux, int *uy)
{
	switch ((angle >> 6) & 3) {
	case 0: *ux = 1; *uy = 0; break;
	case 1: *ux = 0; *uy = 1; break;
	case 2: *ux = -1; *uy = 0; break;
	default: *ux = 0; *uy = -1; break;
	}
}

/* A step of (ux, uy) cells begun, if the cell there is floor. */
static void step(int ux, int uy)
{
	int tx = (view.x >> 8) + ux, ty = (view.y >> 8) + uy;

	if (map_at(tx, ty))
		return;
	left = MOVE_PASSES;
	dx = ux * (CELL / MOVE_PASSES);
	dy = uy * (CELL / MOVE_PASSES);
	dAngle = 0;
}

static void turn(int quarter)
{
	left = MOVE_PASSES;
	dx = dy = 0;
	dAngle = quarter * (64 / MOVE_PASSES);
}

/* What the keys and the joystick ask, once the last move is over. */
static void command(void)
{
	unsigned joy = ms_joystick();
	int ux, uy;

	facing(view.angle, &ux, &uy);
	if (ms_is_held(keys[kForward]) || (joy & MS_JOY_UP))
		step(ux, uy);
	else if (ms_is_held(keys[kBack]) || (joy & MS_JOY_DOWN))
		step(-ux, -uy);
	else if (ms_is_held(keys[kLeft]) || (joy & MS_JOY_LEFT))
		turn(-1);
	else if (ms_is_held(keys[kRight]) || (joy & MS_JOY_RIGHT))
		turn(1);
	else if (ms_is_held(keys[kSideLeft]))
		step(uy, -ux);
	else if (ms_is_held(keys[kSideRight]))
		step(-uy, ux);
}

/* A pass: the keys read, the move advanced or a new one begun, the view
 * drawn.  0 on Q. */
static int pass(void)
{
	int c;

	ms_held_tick(keys, kKeys);
	while ((c = ms_key()) >= 0) {
		if (c == MS_KEY_Q)
			return 0;
		if (c == MS_KEY_T)
			view.textured = !view.textured;
		ms_held_code(keys, kKeys, c);
	}
	if (left > 0) {
		view.x += dx;
		view.y += dy;
		view.angle = (view.angle + dAngle) & 255;
		if (--left == 0) {
			view.x = (view.x & ~255) | (CELL / 2);	/* squarely on the cell */
			view.y = (view.y & ~255) | (CELL / 2);
			view.angle &= ~63;
			hud_compass(view.angle);
		}
	} else {
		command();
	}
	view_render(&view);
	return 1;
}

int main(void)
{
	unsigned last, started;
	int i;

	map_test();
	view.x = map_startX * CELL + CELL / 2;
	view.y = map_startY * CELL + CELL / 2;
	view_init();
	ms_screen_begin(0);
	ms_clock_begin();
	ms_keys_begin();
	hud_frame();
	hud_title("THE CRYPT", 1);
	hud_message("THE AIR IS COLD AND STILL.");
	for (i = 0; i < 4; i++)
		hud_hero(i, &heroes[i]);
	hud_compass(view.angle);
	started = last = ms_frames;
	do {
		while (ms_frames - last < PACE)
			;
		last = ms_frames;
		passes++;
	} while (pass());
	ms_keys_end();
	ms_clock_end();
	ms_screen_end();
	/* How the machine kept up: the frames of the loop alone, the tables
	 * and the screen's making before it left out. */
	printf("CRYPT: %u passes in %u frames\n", passes, ms_frames - started);
	return 0;
}
