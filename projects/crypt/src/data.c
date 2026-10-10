/* data.c - CRYPT.DAT in the banks above the program (data.h). */

#include <ms0515.h>
#include <rt11.h>
#include "data.h"
#include "planes.h"

#define CHANNEL 1

/* The slot's bank: 040000 is bank 2, 060000 bank 3. */
static int bank(int slot)
{
	return plane_slot_address[slot] >> 13;
}

static int selected = -1;

void data_select(int slot)
{
	if (slot == selected)
		return;
	selected = slot;
	ms_bank(bank(slot), plane_slot_extended[slot]);
}

const unsigned char *data_at(int slot, unsigned offset)
{
	return (const unsigned char *)(plane_slot_address[slot] + offset);
}

int data_load(void)
{
	unsigned spec[4];
	int s, n;

	rt11_settop((void *)0077776);		/* the memory above the program is ours */
	rt11_filespec(spec, "CRYPT.DAT");
	if (rt11_lookup(CHANNEL, spec) < PLANE_SLOTS * PLANE_SLOT_BLOCKS)
		return -1;
	for (s = 0; s < PLANE_SLOTS; s++) {
		data_select(s);
		n = rt11_readw(CHANNEL, (unsigned)(s * PLANE_SLOT_BLOCKS),
			       (void *)plane_slot_address[s], PLANE_SLOT_BLOCKS * 256);
		if (n != PLANE_SLOT_BLOCKS * 256) {
			data_select(0);
			return -1;
		}
	}
	data_select(0);				/* the bank primary again */
	rt11_close(CHANNEL);
	return 0;
}
