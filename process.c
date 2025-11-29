#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include "process.h"
#include "prio_q.h"
#include "message.h"

#define MAX_PROCS   100
#define MAX_THREADS 100

enum {
    PROC_NEW = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,        //is process blocked for other reasons
    PROC_BLOCKED_SEND,   //process waiting for send
    PROC_BLOCKED_RECV,   //process waiting for receiving
    PROC_FINISHED
};

static char *states[] = {"new", "ready", "running", "blocked", "blocked (send)", "blocked (recv)", "finished"};
static int quantum;
static prio_q_t *finished;
static barrier_t *gbarrier = NULL; //barrier var

//Setting barrier for processes
extern void process_set_barrier(barrier_t *b) {
    gbarrier = b;
}

/* Initialize the simulation
 * @params:
 *   quantum: the CPU quantum to use in the situation
 * @returns:
 *   returns 1
 */
extern void process_init(int cpu_quantum) {
    quantum = cpu_quantum;
    finished = prio_q_new();
    msg_init();
}

/* Create a new node context
 * @params:
 *   None
 * @returns:
 *   pointer to new node context.
 */
extern processor_t * process_new() {
    processor_t * cpu = calloc(1, sizeof(processor_t));
    assert(cpu);
    cpu->blocked = prio_q_new();
    cpu->ready = prio_q_new();
    cpu->next_proc_id = 1;
    cpu->node_id = 0;
    return cpu;
}

/* Admit a process into the simulation
 * @params:
 *   proc: pointer to the program context of the process to be admitted
 *   cpu : node context
 * @returns:
 *   returns 1
 */
static void print_process(processor_t *cpu, context *proc) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    int result = pthread_mutex_lock(&lock);
    assert(result == 0);
    printf("[%2.2d] %5.5d: process %d %s\n", proc->thread, cpu->clock_time,
           proc->id, states[proc->state]);
    result = pthread_mutex_unlock(&lock);
    assert(result == 0);
}

/* Add process to finished queue when they are done
 * @params:
 *   proc: pointer to the program context of the finished process
 *   cpu : node context
 * @returns:
 *   returns 1
 */
static void process_finished(processor_t *cpu, context *proc) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    /* Need to protect shared queue global lock
     * threads are ordered by time, thread id, proc id.
     */
    proc->finished = cpu->clock_time;
    int result = pthread_mutex_lock(&lock);
    assert(result == 0);
    int order = cpu->clock_time * MAX_PROCS * MAX_THREADS + proc->thread * MAX_PROCS + proc->id;
    prio_q_add(finished, proc, order);
    result = pthread_mutex_unlock(&lock);
    assert(result == 0);
}

/* Compute priority of process, depending on whether SJF or priority based scheduling is used
 * @params:
 *   proc: process' context
 * @returns:
 *   priority of process
 */
static int actual_priority(context *proc) {
    if (proc->priority < 0) {
        /* SJF means duration of current DOOP is the priority
         */
        return proc->duration;
    }
    return proc->priority;
}

/* Insert process into appropriate queue based on the primitive it is performing
 * @params:
 *   proc: process' context
 *   cpu : node context
 *   next_op: if true, current primitive is done, so move IP to next primitive.
 * @returns:
 *   none
 */
static void insert_in_queue(processor_t *cpu, context *proc, int next_op) {
    /* If current primitive is done, move to next
     */
    if (next_op) {
        context_next_op(proc);

    }

    int op = context_cur_op(proc);

    if (op == OP_DOOP || op == OP_SEND || op == OP_RECV) {
        proc->state = PROC_READY;
        /* duration meaning:
           - DOOP : remaining ticks of DOOP
           - SEND/RECV : treat as 1 for SJF purposes so they don't starve
        */
        if (op == OP_DOOP) {
            proc->duration = context_cur_duration(proc);
        } else {
            proc->duration = 1; /* one CPU tick before it blocks on the message op */
        }
        prio_q_add(cpu->ready, proc, actual_priority(proc));
        proc->wait_count++;
        proc->enqueue_time = cpu->clock_time;
    } else if (op == OP_BLOCK) {
        /* Use the duration field of the process to store their wake-up time.
         */
        proc->state = PROC_BLOCKED;
        proc->duration = cpu->clock_time + context_cur_duration(proc); /* wake-up time */
        prio_q_add(cpu->blocked, proc, proc->duration);
    } else {
        proc->state = PROC_FINISHED;
        process_finished(cpu, proc);
    }
    print_process(cpu, proc);
}

/* Admit a process into the simulation
 * @params:
 *   proc: pointer to the program context of the process to be admitted
 *   cpu : node context
 * @returns:
 *   returns 1
 */
extern int process_admit(processor_t *cpu, context *proc) {
    proc->id = cpu->next_proc_id++;
    proc->state = PROC_NEW;
    print_process(cpu, proc);

    //register this (node, pid) address for message passing
    msg_register(cpu->node_id, proc);

    insert_in_queue(cpu, proc, 1);
    return 1;
}

/* Perform the simulation
 * @params:
 *   cpu : node context
 * @returns:
 *   returns 1
 */
extern int process_simulate(processor_t *cpu) {
    context *cur = NULL;
    int cpu_quantum = 0;

    //sync all nodes before starting simulation loop
    if (gbarrier) barrier_wait(gbarrier);

    while (!prio_q_empty(cpu->ready) || !prio_q_empty(cpu->blocked) ||
           cur != NULL || msg_has_blocked_or_ready(cpu->node_id)) {
        int preempt = 0;

        /* Step 1(a): Unblock processes that completed SEND/RECV */
        {
            context *done[256];
            int n = msg_collect_ready(cpu->node_id, done, 256);
            for (int i = 0; i < n; i++) {
                context *p = done[i];
                insert_in_queue(cpu, p, 1); //treat like DOOP for queueing
                preempt |= cur != NULL && p->state == PROC_READY &&
                           actual_priority(cur) > actual_priority(p);
            }
        }

        //Step 1(b): Unblock processes whose BLOCK time expired
        while (!prio_q_empty(cpu->blocked)) {
            context *proc = prio_q_peek(cpu->blocked);
            if (proc->duration > cpu->clock_time) {
                break;
            }
            prio_q_remove(cpu->blocked);
            insert_in_queue(cpu, proc, 1);
            preempt |= cur != NULL && proc->state == PROC_READY &&
                       actual_priority(cur) > actual_priority(proc);
        }

        if (gbarrier) barrier_wait(gbarrier);

        /* Step 2: Update current running process */
        if (cur != NULL) {
            int op = context_cur_op(cur);
            if (op == OP_DOOP) {
                cur->duration--;
                cpu_quantum--;
                if (cur->duration == 0 || cpu_quantum == 0 || preempt) {
                    insert_in_queue(cpu, cur, (cur->duration == 0));
                    cur = NULL;
                }
            } else if (op == OP_SEND) {
                cpu_quantum--;             /* consume this CPU tick */
                cur->doop_time++;          /* count as running time (matches assignment examples) */
                msg_send(cur, context_cur_duration(cur));
                cur->state = PROC_BLOCKED_SEND;
                print_process(cpu, cur);
                cur = NULL;
            } else if (op == OP_RECV) {
                cpu_quantum--;             /* consume this CPU tick */
                cur->doop_time++;          /* count as running time */
                msg_recv(cur, context_cur_duration(cur));
                cur->state = PROC_BLOCKED_RECV;
                print_process(cpu, cur);
                cur = NULL;
            } else {
                //shouldn't run BLOCK/HALT here
                assert(0 && "Invalid running op");
            }
        }

        if (gbarrier) barrier_wait(gbarrier);

        /* Step 3: Select next ready process to run if none are running
         * Be sure to keep track of how long it waited in the ready queue
         */
        if (cur == NULL && !prio_q_empty(cpu->ready)) {
            cur = prio_q_remove(cpu->ready);
            cur->wait_time += cpu->clock_time - cur->enqueue_time;
            cpu_quantum = quantum;
            cur->state = PROC_RUNNING;
            print_process(cpu, cur);
        }

        /* Step 4: barrier + increment clock */
        if (gbarrier) barrier_wait(gbarrier);
        cpu->clock_time++;
    }

    return 1;
}

/* Output process summary post execution
 * @params:
 *   fout : output file
 * @returns:
 *   none
 */
extern void process_summary(FILE *fout) {
    while (!prio_q_empty(finished)) {
        context *proc = prio_q_remove(finished);
        context_stats(proc, fout);
    }
}
