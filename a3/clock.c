#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_arm;

int clock_evict() {
	// keep iterating until a off bit found
	while (1){
		clock_arm = (clock_arm + 1) % memsize;
		int ref_bit_on = coremap[clock_arm].pte->frame & PG_REF;
		if (ref_bit_on == 0) { break; } // the REF bit is off
		coremap[clock_arm].pte->frame &= ~PG_REF; // the REF bit become 0
	}
	return clock_arm;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm.
 */
void clock_init() {
	clock_arm = 0;
	return;
}
