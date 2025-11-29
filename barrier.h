//filename: barrier.h
//author: Arash Tashakori - B00872075
//Description: This barrier synchronizes a dynamic set of threads. It has 2 condition variables (phase 0 / phase 1)
// to avoid the "re-enter" bug where a thread that just passed a barrier can loop around and wake itself up before
// others have left the last barrier phase

#ifndef PROSIM_BARRIER_H
#define PROSIM_BARRIER_H
#include <pthread.h>

typedef struct {
    pthread_mutex_t m;       //mutex for all the fields below
    pthread_cond_t cv[2];   // CVs
    int phase;              //current phase: 0 or 1
    int max_threads;        //total number of threads expected at the barrier
    int cur_threads;        //# of threads waiting in this phase right now
} barrier_t;

//initialize the barrier with an initial # of threads
void barrier_init(barrier_t *b, int n);

//blocks until all current threads  arrive
void barrier_wait(barrier_t *b);

//remove one thread from the barrier when done
void barrier_done(barrier_t *b);

#endif
