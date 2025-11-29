//filename: message.c
//author: Arash Tashakori - B00872075
//Description: This file implements synchronous message passing for the simulator

#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "message.h"
#include "prio_q.h"

// bounds for the synchronization
#define MSG_MAX_THREADS 100
#define MSG_MAX_PROCS   100

//address = node*100 + pid
#define MSG_MAX_ADDR    ((MSG_MAX_THREADS+2)*100 + (MSG_MAX_PROCS+2))

typedef struct {
    pthread_mutex_t lock;
    int init;
    int waiting_type;     /* 0 none, 1 waiting SEND, 2 waiting RECV */
    int partner_addr;     /* the address it is paired/waiting for */
    int node_id;          /* node owning this address */

    context *ctx;         /* context pointer */
} endpoint_t;

typedef struct {
    pthread_mutex_t lock;
    prio_q_t *ready;      /* matched processes to unblock next tick  */
} pernode_t;

static endpoint_t ep[MSG_MAX_ADDR];
static pernode_t  pernode[MSG_MAX_THREADS+1];

static inline int _addr_of(context *c) {
    return c->thread * 100 + c->id;
}

//init helpers
static void _init_ep(endpoint_t *e) {
    if (!e->init) {
        pthread_mutex_init(&e->lock, NULL);
        e->init = 1;
    }
}

//initializing nodes
static void _init_node(int node_id) {
    if (!pernode[node_id].ready) {
        pthread_mutex_init(&pernode[node_id].lock, NULL);
        pernode[node_id].ready = prio_q_new();
    }
}

//initializing messages
void msg_init(void) {
    memset(ep, 0, sizeof(ep));
    memset(pernode, 0, sizeof(pernode));
}

//register a process so that node,pid pair to context is clear later
void msg_register(int node_id, context *proc) {
    int addr = node_id * 100 +  proc->id;
    assert(addr >= 0 && addr < MSG_MAX_ADDR);

    _init_ep(&ep[addr]);
    _init_node(node_id);

    pthread_mutex_lock(&ep[addr].lock);

    ep[addr].waiting_type = 0; //not waiting yet
    ep[addr].partner_addr = 0;

    ep[addr].node_id = node_id;
    ep[addr].ctx = proc;

    pthread_mutex_unlock(&ep[addr].lock);
}

//Push a context to its node's completion list (ordered by PID)
static void _push_done(context *c) {
    int node_id = c->thread;
    _init_node(node_id);

    pthread_mutex_lock(&pernode[node_id].lock);

    prio_q_add(pernode[node_id].ready, c, c->id); //lower PID first

    pthread_mutex_unlock(&pernode[node_id].lock);
}

//lock two endpoints in ascending address order (for deadlock prevention
static void _lock_two(endpoint_t *a, int aaddr, endpoint_t *b, int baddr) {
    if (aaddr == baddr) {
        pthread_mutex_lock(&a->lock);
        return;
    }

    if (aaddr < baddr) {
        pthread_mutex_lock(&a->lock);
        pthread_mutex_lock(&b->lock);
    } else {
        pthread_mutex_lock(&b->lock);
        pthread_mutex_lock(&a->lock);
    }
}

//unlock in reverse order
static void _unlock_two(endpoint_t *a, endpoint_t *b, int same) {
    if (same) {
        pthread_mutex_unlock(&a->lock);
        return;
    }

    pthread_mutex_unlock(&a->lock);
    pthread_mutex_unlock(&b->lock);
}

//this method handles sending the message
void msg_send(context *sender, int receiver_addr) {
    int saddr = _addr_of(sender); //sender's address

    assert(receiver_addr >= 0 && receiver_addr < MSG_MAX_ADDR);

    _init_ep(&ep[saddr]);
    _init_ep(&ep[receiver_addr]);

    endpoint_t *se = &ep[saddr];
    endpoint_t *re = &ep[receiver_addr];

    _lock_two(se, saddr, re, receiver_addr);

    int same = (saddr == receiver_addr); //if the sender and receiver are the same

    //if receiver is already waiting for this sender, both are completed
    if (re->waiting_type == 2 && re->partner_addr == saddr && re->ctx) {
        //mark send as done
        re->waiting_type = 0;
        re->partner_addr = 0;

        //adjust the count for send and receive
        sender->send_count++;
        re->ctx->recv_count++;

        _push_done(sender);
        _push_done(re->ctx);
    } else {
        // sender should be waiting for receiver
        se->waiting_type = 1;
        se->partner_addr = receiver_addr;
    }

    _unlock_two(se, re, same);
}



//this method handles receiving messages
void msg_recv(context *receiver, int sender_addr) {
    int raddr = _addr_of(receiver);
    assert(sender_addr >= 0 && sender_addr < MSG_MAX_ADDR);

    _init_ep(&ep[raddr]);
    _init_ep(&ep[sender_addr]);

    endpoint_t *re = &ep[raddr];
    endpoint_t *se = &ep[sender_addr];

    _lock_two(re, raddr, se, sender_addr);
    int same = (raddr == sender_addr);

    //If sender is already waiting for this receiver, both are completed
    if (se->waiting_type == 1 && se->partner_addr == raddr && se->ctx) {
        se->waiting_type = 0;
        se->partner_addr = 0;

        receiver->recv_count++;
        se->ctx->send_count++;

        _push_done(receiver);
        _push_done(se->ctx);
    } else {
        //otherwise receiver waits for a specific sender
        re->waiting_type = 2;
        re->partner_addr = sender_addr;
    }

    _unlock_two(re, se, same);
}

// pulls completions for thi node in ascending order of their pid
int msg_collect_ready(int node_id, context **out, int maxn) {
    _init_node(node_id);
    pthread_mutex_lock(&pernode[node_id].lock);

    int n = 0;

    while (n < maxn  && !prio_q_empty(pernode[node_id].ready)) {
        out[n++] = (context*) prio_q_remove(pernode[node_id].ready);
    }

    pthread_mutex_unlock(&pernode[node_id].lock);
    return n;
}


//true if this node is not fully completed yet or any process is blocked
int msg_has_blocked_or_ready(int node_id) {
    _init_node(node_id);

    // Any completions queued for this node?
    pthread_mutex_lock( &pernode[node_id].lock);
    int has = !prio_q_empty(pernode[node_id].ready);
    pthread_mutex_unlock(&pernode[node_id].lock);

    if (has) return 1;

    //Any addresses on this node currently waiting on a SEND/RECV left?
    for (int addr = 0;  addr < MSG_MAX_ADDR; ++addr) {
        if (!ep[addr].init)  continue;
        pthread_mutex_lock(&ep[addr].lock);


        if (ep[addr].ctx && ep[addr].ctx->thread == node_id && ep[addr].waiting_type != 0) {
            pthread_mutex_unlock(&ep[addr].lock);

            return 1;
        }

        pthread_mutex_unlock(&ep [addr].lock);
    }
    return 0;
}
