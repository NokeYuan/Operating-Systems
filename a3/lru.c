#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int *record;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int t;

int lru_evict() {
	int victim = 0;
	long unsigned min = 999999999999;
	// loop to update the min value
	for (int fn = 0; fn < memsize; fn++) {
		if ((record[fn] > 0) && (record[fn] < min)) {
			victim = fn;
			min = record[fn];
		}
	}
	return victim;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
	int fn = p->frame >> PAGE_SHIFT;
	t += 1;
	record[fn] = t;
	return;
}


/* Initialize any data structures needed for this
 * replacement algorithm
 */
void lru_init() {
	t = 0;
	record = malloc(sizeof(int) * memsize);
	for (int i = 0; i < memsize; i++) { record[i] = -1; }
}
