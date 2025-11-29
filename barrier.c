//filename: barrier.c
//author: Arash Tashakori - B00872075
//Description: this file implements a two-phased barrier using a mutex + two condition vars.
//The last arriver flips phase and wakes everyone waiting on the old phase

#include "barrier.h"

//This method initializes barrier states
void barrier_init(barrier_t *b, int n) {
    //initialize mutex
    pthread_mutex_init(&b->m, NULL);

    //initialize CVs for phase 0 and 1
    pthread_cond_init(&b->cv[0], NULL);
    pthread_cond_init(&b->cv[1], NULL);

    b->phase = 0;
    b->max_threads = n; //threads that must arrive
    b->cur_threads = 0; //threads that have arrived
}

//this method waits at the barrier. it increments arrival count and waits on cv if the last one hasn;t arrived
void barrier_wait(barrier_t *b) {
    pthread_mutex_lock(&b->m); //enter monitor

    int my_phase = b->phase;
    b->cur_threads++;   //this thread arrived

    if (b->cur_threads < b->max_threads) {
        //as long as the last thread hasn't come yet, wait here
        while (my_phase == b->phase) {
            pthread_cond_wait(&b->cv[my_phase], &b->m);
        }
    } else {
        //last thread to arrive
        b->cur_threads = 0;
        b->phase ^= 1; //flip the phase
        pthread_cond_broadcast(&b->cv[my_phase]); //wake everything waiting on last phase
    }

    pthread_mutex_unlock(&b->m); //leave monitor
}

//it signals that the last thread is done with the barrier.
void barrier_done(barrier_t *b) {
    pthread_mutex_lock(&b->m);  //monitor

    if (b->max_threads > 0){ //make sure it doesn't get negative
        b->max_threads--;   //decrease number of threads
    }

    //if the number of waiting threads (cur_threads) now equals new max we must release them
    if (b->cur_threads == b->max_threads && b->max_threads > 0) {
        int my_phase = b->phase;
        b->cur_threads = 0;
        b->phase ^= 1;
        pthread_cond_broadcast(&b->cv[my_phase]);   //release the waiting thread
    }

    pthread_mutex_unlock(&b->m); //leave monitor
}
