/* banks.c - the extended memory banks.
 *
 * docs/programming.md, "Memory and the monitor": 56 KB of RAM in seven
 * primary banks of 8 KB (dispatcher bits 0-6, 1 = primary), seven more
 * extended behind them; RT-11 sees the primary banks only, the extended
 * ones are a program's own (FIST keeps its game state there). */

#include <ms0515.h>
#include "internal.h"

void ms_bank(int n, int extended)
{
	static const unsigned bits[7] = {1, 2, 4, 8, 16, 32, 64};	/* not 1 << n: the compiler's third trap */
	unsigned bit = bits[n & 7];

	if (extended)
		ms_dispatcher_clear(bit);
	else
		ms_dispatcher_set(bit);
}
