#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"

extern struct intersection isection;

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with
 * its in_direction
 *
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
    int id;
    struct car *cur_car;
    struct lane *cur_lane;
    enum direction in_dir, out_dir;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {

        /* construct car */
        cur_car = malloc(sizeof(struct car));
        cur_car->id = id;
        cur_car->in_dir = in_dir;
        cur_car->out_dir = out_dir;

        /* append new car to head of corresponding list */
        cur_lane = &isection.lanes[in_dir];
        cur_car->next = cur_lane->in_cars;
        cur_lane->in_cars = cur_car;
        cur_lane->inc++;
    }

    fclose(f);
}

/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 *
 */
void init_intersection() {
    int i=0;
    for (i=0; i<4; i++) {
        isection.lanes[i].inc = 0;
        isection.lanes[i].passed = 0;
	    isection.lanes[i].buffer = malloc(LANE_LENGTH*(sizeof(struct car*)));
        isection.lanes[i].head = 0;
        isection.lanes[i].tail = 0;
        isection.lanes[i].in_buf = 0;
        isection.lanes[i].capacity = LANE_LENGTH;
        isection.lanes[i].in_cars = NULL;
        isection.lanes[i].out_cars = NULL;
        pthread_cond_init(&(isection.lanes[i].consumer_cv), NULL);
        pthread_cond_init(&(isection.lanes[i].producer_cv), NULL);
        pthread_mutex_init(&(isection.lanes[i].lock), NULL);
        pthread_mutex_init(&(isection.quad[i]), NULL);
	}
}

/**
 * TODO: Fill in this function
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 *
 */
void *car_arrive(void *arg) {
    struct lane *l = arg;

    /* avoid compiler warning */
    l = l;
    pthread_mutex_lock(&l->lock);
    int have_cars = (l->inc != 0);
    while(have_cars) {
        int buffer_full = (l->in_buf == l->capacity);
        while(buffer_full){
            pthread_cond_wait(&(l->producer_cv), &(l->lock));
            buffer_full = (l->in_buf == l->capacity);
        }
        l->buffer[l->tail] = l->in_cars;

        /* was full before sending the buffer */
        if (l->tail + 1 == l->capacity) { l->tail = 0; }
        else { l->tail++; }

        /* buffer: list of car in line */
        /* tail points to the last car in lane that will enter intersection */
        /* assign head of in car to buffer tail */

        l->in_buf++;
        l->inc--;
        have_cars = (l->inc != 0);
        l->in_cars = (l->in_cars)->next;
        pthread_cond_signal(&(l->consumer_cv));
    }
    pthread_mutex_unlock(&(l->lock));
    return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list of the lane that corresponds to the car's
 * out_dir. Do not free the cars!
 *
 *
 * Note: For testing purposes, each car which gets to cross the
 * intersection should print the following three numbers on a
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 *
 * You may add other print statements, but in the end, please
 * make sure to clear any prints other than the one specified above,
 * before submitting your final code.
 */
void *car_cross(void *arg) {
    struct lane *l = arg;
    /* avoid compiler warning */
    l = l;

    struct car *current_car;
    pthread_mutex_lock(&l->lock);
    int *quadrants_path;
    int wait_for_coming_cars = (l->in_buf == 0);
	int have_cars = (l->in_buf != 0||l->inc != 0);
    while(have_cars){
        while(wait_for_coming_cars){
            pthread_cond_wait(&l->consumer_cv, &l->lock);
            wait_for_coming_cars = (l->in_buf == 0);
        }
        current_car = l->buffer[l->head];
        l->passed++;
        l->in_buf--;

        /* adjust head index*/
        if (l->head + 1 == l->capacity) { l->head = 0; }
     	else { l->head++; }
     	quadrants_path = compute_path(current_car->in_dir, current_car->out_dir);
     	int i;
        for (i=0;i<4;i++) {
            if (quadrants_path[i] != -1) {
                pthread_mutex_lock(&isection.quad[quadrants_path[i]-1]);
            }
        }
        printf("%d %d %d\n", current_car->in_dir, current_car->out_dir, current_car->id);
        /* link to outline */
        struct lane *out_line= &(isection.lanes[current_car->out_dir]);
        current_car->next = out_line->out_cars;
        out_line->out_cars = current_car;
        for (i=0;i<4;i++) {
 	    	if (quadrants_path[i]!= -1) {
                pthread_mutex_unlock(&isection.quad[quadrants_path[i]-1]);
            }
        }
        have_cars = (l->in_buf != 0||l->inc != 0);
        pthread_cond_signal(&l->producer_cv);
    }
    free(quadrants_path);
    free(l->buffer);
    pthread_mutex_unlock(&l->lock);
    return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted
 * list of the quadrants the car will pass through.
 *
 */
void *assign_path(int *quadrants_path, int a, int b, int c, int d) {
    quadrants_path[0] = a;
    quadrants_path[1] = b;
    quadrants_path[2] = c;
    quadrants_path[3] = d;
    return 0;
}

int *compute_path(enum direction in_dir, enum direction out_dir) {
    int *quadrants_path = malloc(sizeof(int) * 4);
    if (in_dir == NORTH) {
        if (out_dir == WEST) { assign_path(quadrants_path, 2,-1,-1,-1); }
        if (out_dir == SOUTH) { assign_path(quadrants_path, 2,3,-1,-1); }
        if (out_dir == EAST) { assign_path(quadrants_path, 2,3,4,-1); }
    }
    if (in_dir == WEST) {
        if (out_dir == NORTH) { assign_path(quadrants_path, 3,4,1,-1); }
        if (out_dir == SOUTH) { assign_path(quadrants_path, 3,-1,-1,-1); }
        if (out_dir == EAST) { assign_path(quadrants_path, 3,4,-1,-1); }
    }
    if (in_dir == SOUTH) {
        if (out_dir == NORTH) { assign_path(quadrants_path, 4,1,-1,-1); }
        if (out_dir == WEST) { assign_path(quadrants_path, 4,1,2,-1); }
        if (out_dir == EAST) { assign_path(quadrants_path, 4,-1,-1,-1); }
    }
    if (in_dir == EAST) {
        if (out_dir == NORTH) { assign_path(quadrants_path, 1,-1,-1,-1); }
        if (out_dir == WEST) { assign_path(quadrants_path, 1,2,-1,-1); }
        if (out_dir == SOUTH) { assign_path(quadrants_path, 1,2,3,-1); }
    }
    return quadrants_path;
}
