/* keys.c - the keyboard taken from the ROM.
 *
 * docs/programming.md, "The keyboard": the ROM's handler on vector 130
 * takes a byte the moment it arrives and hands it to RT-11, so a
 * program takes the vector with a handler that queues the bytes
 * (keys.s, a ring of 16) and gives it back on exit; it tells the
 * keyboard 231 first, keyclick off, which the firmware takes as "a game
 * runs" and repeats keys sooner.  No key-up codes: a held key is a
 * timer refreshed by its code, two timers for the first code and the
 * repeats (MANICM's KEYS).
 *
 * The keyboard sends a key's code once; while the key stays down it
 * sends 264, the metronome, in place of it (the firmware's L_2B0), and
 * both ROMs repeat the last key themselves on 264.  ms_key does as the
 * ROMs do: a 264 comes out as the code of the last key, so a held key
 * is still a stream of its own code.  The modifiers' codes are not a
 * last key - the keyboard repeats no modifier - and ALL-UP leaves none. */

#include <ms0515.h>
#include "internal.h"

#define KEY_VECTOR 0130
#define KEYCLICK_OFF 0231
#define KEY_CTRL 0257
#define KEY_REPEAT 0264

void ms_key_isr(void);			/* keys.s */
volatile unsigned char ms_keyring[16];	/* keys.s fills it at ms_keyhead */
volatile unsigned ms_keyhead;
static unsigned tail;
static int last;			/* the key a 264 repeats, or -1 */
static struct ms_vector saved;

void ms_keys_begin(void)
{
	ms_keyhead = 0;
	tail = 0;
	last = -1;
	ms_vector_take(KEY_VECTOR, ms_key_isr, &saved);
	while (!(*MS_KEY_STATUS & 1))	/* until the USART can send */
		;
	*MS_KEY_DATA = KEYCLICK_OFF;
}

void ms_keys_end(void)
{
	ms_vector_give(KEY_VECTOR, &saved);
}

int ms_key(void)
{
	int c;

	for (;;) {
		if (tail == ms_keyhead)
			return -1;
		c = ms_keyring[tail];
		tail = (tail + 1) & 15;
		if (c == KEY_REPEAT) {
			if (last >= 0)
				return last;
			continue;		/* a repeat of nothing we saw */
		}
		if (c == MS_KEY_ALL_UP)
			last = -1;
		else if (c != MS_KEY_SHIFT && c != KEY_CTRL)
			last = c;
		return c;
	}
}

void ms_keys_flush(void)
{
	tail = ms_keyhead;
}

void ms_held_tick(struct ms_held *keys, int n)
{
	for (; n > 0; n--, keys++)
		if (keys->frames)
			keys->frames--;
}

int ms_held_code(struct ms_held *keys, int n, int code)
{
	for (; n > 0; n--, keys++)
		if (keys->code == code) {
			keys->frames = keys->frames ? MS_HELD_NEXT : MS_HELD_FIRST;
			return 1;
		}
	return 0;
}
