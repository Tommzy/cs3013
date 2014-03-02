/**
* Adapted from CS-502 Project #3, Fall 2006
*	originally submitted by Cliff Lindsay
* Modified for CS-3013, C-term 2012
*
*/

#ifndef __MAILBOX__
#define __MAILBOX__

#include <stdbool.h>
#include <linux/types.h>

#define NO_BLOCK 0
#define BLOCK   1
#define MAX_MSG_SIZE 128
#define HASHSIZE 100
/*
 * Functions for msgs
 * 
 */
asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block);
asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block);
/*
 * functions for maintaining mailboxes
 * 
 */
asmlinkage long ManageMailbox(bool stop, int *count);

/*
 * error codes pertaining to mailboxes
 * 
 */
#define MAILBOX_FULL		1001
#define MAILBOX_EMPTY		1002
#define MAILBOX_STOPPED		1003
#define MAILBOX_INVALID		1004
#define MSG_LENGTH_ERROR	1005
#define MSG_ARG_ERROR		1006
#define MAILBOX_ERROR		1007
#define alloc_CS3013_message() ((CS3013_message *)kmem_cache_alloc(CS3013_message_cachep, GFP_KERNEL))
#define free_CS3013_message(msg) kmem_cache_free(CS3013_message_cachep, (msg))
static struct kmem_cache *CS3013_message_cachep;

typedef struct CS3013_mailbox {
 struct list_head *messages;
 bool stopped;
 struct semaphore *empty;
 struct semaphore *full;
 int waitingSenders;
 int waitingReceivers;
}

typedef struct CS3013_message {
 struct list_head list;
 pid_t sender_pid;
 char text[MAX_MSG_SIZE];
 int length;
}


#endif

