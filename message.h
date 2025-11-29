//filename: message.h
//author: Arash Tashakori - B00872075
//Description: This is the header file for the synchronizing messaging
#ifndef PROSIM_MESSAGE_H
#define PROSIM_MESSAGE_H

#include "context.h"



//initializes the message-passing global state.
void msg_init(void);

//this registers a process' node and pid for address mapping once it has a PID
void msg_register(int node_id, context *proc);

//synchronous primitives - called by a process currently running on its ticks
void msg_send(context *sender, int receiver_addr);
void msg_recv(context *receiver, int sender_addr);

//this method collects locally completed send/recvs (ordered by PID). returns the count
int msg_collect_ready(int node_id, context **out, int maxn);

//this method returns true if this node has any msg-completions pending or procs blocked in SEND/RECV
int msg_has_blocked_or_ready(int node_id);

#endif
