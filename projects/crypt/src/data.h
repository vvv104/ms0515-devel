/* data.h - CRYPT.DAT in the banks above the program.
 *
 * The side walls drawn on the host (art/planes.py) come as slots of
 * 8 KB, one for each half of the bank at 060000: the primary half,
 * which RT-11 left free above the program, and the extended one behind
 * it.  The bank shows one half at a time, so a slot is selected before
 * its words are read; the program itself lies under 060000 (the build
 * checks) and is never hidden. */

#ifndef CRYPT_DATA_H
#define CRYPT_DATA_H

/* CRYPT.DAT read from the program's folder into the slots, before the
 * screen begins (the monitor does the reading).  0, or -1 when the
 * file is missing or short. */
int data_load(void);
/* The slot's half of its bank shown: its words at data_at from here
 * until another slot of the same bank is selected. */
void data_select(int slot);
/* The address of the slot's byte at `offset`. */
const unsigned char *data_at(int slot, unsigned offset);

#endif
