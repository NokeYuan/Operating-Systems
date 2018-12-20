#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

#define MAXLINE 128

int *next_ref;

int addr_len = 0;

int *ref;

int *addr;

int cur_addr = 0;

extern char *tracefile;

/* Page to evict is chosen using the optimal (aka MIN) algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	int frame = 0;
	int opt = 0;
	int i = 0;
	// initialize next ref
	while (i<memsize) {
		next_ref[i] = addr_len; i++;
	} i = 0;
	// update value of next ref
	while (i<memsize) {
		int j = cur_addr;
		while (j<addr_len) {
			if (ref[j] == addr[i]) {
				next_ref[i] = j; break; } j++;
			} i++;
	} i = 0;
	// update value of opt
	while (i<memsize) {
		if (next_ref[i]>opt) {
			frame = i; opt = next_ref[i];
		} i++;
	}
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
	int frame = p->frame>>PAGE_SHIFT;
	addr[frame] = ref[cur_addr];
	cur_addr += 1;
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
	// allocate space
	next_ref = malloc(memsize * sizeof(int));
	addr = malloc(memsize * sizeof(int));

	// open file
	FILE *fp;
	char buffer[MAXLINE];
	addr_t trace = 0;
	char type;

	fp = fopen(tracefile, "r");
	if (fp == NULL) { exit(1); }

	while (fgets(buffer, MAXLINE, fp) != NULL) { addr_len += 1; }
	ref = malloc(addr_len * sizeof(int));
	// reset file pointer to the beginning
	fseek(fp, 0, SEEK_SET);

	int i = 0;
	while (fgets(buffer, MAXLINE, fp) != NULL) {
  	if (buffer[0] == '=') { break; }
		sscanf(buffer, "%c %lx", &type, &trace);
		ref[i] = trace; i++;
  }
	fclose(fp);
}
