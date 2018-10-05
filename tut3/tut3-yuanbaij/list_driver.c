#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "list.h"

#define MAXRUNS 10000
#define NUMTHREADS 8

struct list L;

struct counter {
    int inserts;
} count[NUMTHREADS];

// Insert MAXRUNS random numbers into list L
void *worker(void *threadid) {
    long id = (long) threadid;
    int i;
    
    for(i = 0; i < MAXRUNS; i++) {
        int val = random() % MAXRUNS;
        insert(&L, val);
        count[id].inserts++;
    }
    //print_list(&L);
    printf("Thread %ld inserted %d values\n", id, count[id].inserts);
        
    pthread_exit(NULL);
}

void simple_test() {
    insert(&L, 10);
    insert(&L, 2);
    insert(&L, 3);
    insert(&L, 5);
    insert(&L, 15);
    insert(&L, 12);
    print_list(&L);
}




int main(int argc, char **argv) {
    pthread_t threads[NUMTHREADS];
    long tid;
    int err;

    if(argc == 2) {
        if(argv[1][0] == '-' && argv[1][1] == 's') {
            simple_test();
            exit(0);
        } else {
            printf("invalid argument %s\n", argv[1]);
            exit(0);
        }
    }

    for(tid = 0; tid < NUMTHREADS; tid++) {
        count[tid].inserts = 0;
    }
     pthread_mutex_init(&L.lock, NULL);
    
    for (tid = 0; tid < NUMTHREADS; tid++) {
        err = pthread_create(&threads[tid], NULL, worker, (void *)tid);
        if (err) {
            printf("Error: pthread_create failed %li.\n", tid);
            return 1;
        }
    }
    for (tid = 0; tid < NUMTHREADS; tid++) {
        err = pthread_join(threads[tid], NULL);
        if(err) {
            printf("Error: pthread_join failed %li.\n", tid);
            return 1;
        }
    }
    int sum_inserts = 0;
    for(tid = 0; tid < NUMTHREADS; tid++) {
        sum_inserts += count[tid].inserts;
    }
    //print_list(&L);
    printf("Total inserts = %d\n", sum_inserts);
    printf("Final length = %d\n", length(&L));

    
    return 0;
}
