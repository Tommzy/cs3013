// We need to define __KERNEL__ and MODULE to be in Kernel space
// If they are defined, undefined them and define them again:
#undef __KERNEL__
#undef MODULE

#define __KERNEL__
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mailbox.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <stdbool.h>

#define HASH_TABLE_SIZE 100
#define MAX_MAILBOX_MSG_NUM 32
//#define MAX_MAILBOX_SIZE 64
//#define MAX_MAILBOXES 32
#define FALSE 0
#define TRUE 1

/**
 * Hashtable implementation
 *
 * */
typedef struct message_s {
	int len;
	pid_t sender;
	char msg[MAX_MSG_SIZE];
} message;

typedef struct mailbox_s {
	int pid; // PID
	int msgNum; // Number of messages
	int ref_counter; // Ref counter for when a mailbox is stopped
	bool stopped;
	message *messages[32];
	wait_queue_head_t read_queue;
	wait_queue_head_t write_queue;
} mailbox; // struct mailbox

typedef struct HashEntry_s {
	pid_t pid;
	mailbox* mb;
	wait_queue_head_t wq;
	struct HashEntry_s* next;

}HashEntry;

typedef struct hashtable_s {
	struct HashEntry_s *hE[100];
	spinlock_t main_lock;
} hashtable; // struct hashtable



extern struct kmemfind_cache *cache;
unsigned long **sys_call_table;
struct kmem_cache *message_cache = NULL;
struct kmem_cache *mailbox_cache = NULL;

hashtable *ht;
static spinlock_t main_lock_t;


asmlinkage long (*ref_cs3013_syscall1)(void);
asmlinkage long (*ref_cs3013_syscall2)(void);
asmlinkage long (*ref_cs3013_syscall3)(void);
asmlinkage long (*ref_sys_exit)(int error_code);
asmlinkage long (*ref_sys_exit_group)(int error_code);

int insertMsg(pid_t dest, void *msg, int len, bool block);
int removeMsg(pid_t *sender, void *msg, int *len, bool block);
HashEntry *getEntry(pid_t pid);
void doExit(void);
int remove(int pid);
mailbox *createMailbox(pid_t pid);

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	int err;

	printk(KERN_INFO "SendMsg: sending message");

	err = insertMsg(dest, msg, len, block);
	printk(KERN_INFO "SendMsg: insertMsg message success");

	// Error in sending the message
	if(err != 0){
		printk(KERN_INFO "SendMsg: Error inserting message, error code %d\n", err);
		return err;
	}

	return 0;
}	// asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block)

asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	int err;
	printk(KERN_INFO "RcvMsg: Receiving from PID %d", current->pid);

	err = removeMsg(sender, msg, len, block);

	// Error in removing the message
	if(err != 0){
		printk(KERN_INFO "RcvMsg: Error removing message, error code %d\n", err);
		return err;
	}

	return 0;
}	// asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block)

long ManageMailbox(bool stop, int *count){
	HashEntry *he = getEntry(current->pid);
	mailbox *mb = he->mb;

	spin_lock(&((&(he->wq))->lock));

	if(mb == NULL){
		return MAILBOX_INVALID;
	}

	copy_to_user(count, &mb->msgNum, sizeof(int)); // Copy the count to user
	mb->stopped = stop; // Copy boolean value from user

	spin_unlock(&((&(he->wq))->lock));

	return 0;
}	// asmlinkage long ManageMailbox(bool stop, int *count)


asmlinkage long MailboxExit(int error_code){
	doExit();
	return ref_sys_exit(error_code);
} // asmlinkage long MailboxExit(void)

asmlinkage long MailboxExitGroup(int error_code){
	doExit();
	return ref_sys_exit_group(error_code);
} // asmlinkage long MailboxExitGroup(int error_code)

void doExit(void){
	remove(current->pid);
}

int create(void){
	int i;
	// Allocate space for hashtable
	ht = (hashtable *)kmalloc(sizeof(hashtable), GFP_KERNEL);
	if(ht == NULL){
		printk(KERN_INFO "kmalloc hastable Failure");
		return 1;
	}
	//init the hashtable lock
	spin_lock_init(&ht->main_lock);

	printk(KERN_INFO "Initializing hashtable!");
	// Initialize hashtable
	for(i = 0; i < 100; i++){
		ht->hE[i] = NULL;
	}
	printk(KERN_INFO "11111111111111111111111111111111111111111111111111111111");
	return 0;
} // hashtable *create(void)

HashEntry* createHashEntry(pid_t pid){
	HashEntry* newHashEntry;
	printk(KERN_INFO "Initializing HashEntry!");

	newHashEntry =	(HashEntry*)kmalloc(sizeof(HashEntry), GFP_KERNEL);
	newHashEntry->pid = pid;
	newHashEntry->mb = createMailbox(pid);

	//	printk(KERN_INFO "In createHashEntry: init_waitqueue_head(&newHashEntry->wq)!");
	init_waitqueue_head(&newHashEntry->wq);
	newHashEntry->next = NULL;
	return newHashEntry;
}
mailbox *createMailbox(pid_t pid){
	mailbox *newBox;
	int i;
	printk(KERN_INFO "createMailbox: Initializing mailbox!ID:%d", pid);

	//	printk(KERN_INFO "createMailbox: malloc newmailbox!");
	//	mailbox *newBox = (mailbox*)kmalloc(sizeof(mailbox), GFP_KERNEL); // Allocate mailbox from cache
	newBox = kmem_cache_alloc(mailbox_cache, GFP_KERNEL);
	// Init values
	newBox->pid = pid;
	newBox->ref_counter = 0;
	newBox->msgNum = 0;
	newBox->stopped = false;
	//TODO next is not in the mailbox
	printk(KERN_INFO "createMailbox: waitqueue read!");
	init_waitqueue_head(&newBox->read_queue);
	printk(KERN_INFO "createMailbox: waitqueue wait!");
	init_waitqueue_head(&newBox->write_queue);

	// Initialize messages to NULL
	for(i = 0; i < MAX_MAILBOX_MSG_NUM; i++){
		newBox->messages[i] = NULL;
	}

	return newBox;
} // mailbox *createMailbox(int pid)
int hashfunc(pid_t pid){
	printk(KERN_INFO "hash: pid: %d!",pid);
	int hashval = (int) pid % 100;
	printk(KERN_INFO "hash: hashval: %d!", hashval);
	return hashval;
}
HashEntry *getEntry(pid_t pid){
	HashEntry* ahashentry;
	HashEntry *temphE;
	HashEntry *he;
	int pide = pid;
	int find;
	int hashvalue;
	printk(KERN_INFO "getEntry: init pre success! and prepare to generate hashvalue");
	hashvalue = hashfunc(pide);

	printk(KERN_INFO "getEntry: start!");

	spin_lock(&ht->main_lock);
	printk(KERN_INFO "getEntry: hashval: %d!", hashvalue);
	//TODO double check here
	temphE = ht->hE[hashvalue];
	//	printk(KERN_INFO "getEntry: temphE PID:%d!", temphE->pid);
	if( temphE == NULL) {
		printk(KERN_INFO "createMailbox: !!!!!!!");
		ahashentry = createHashEntry(pide);
		ht->hE[hashvalue] = ahashentry;
	}else{
		find = 0;
		while (temphE != NULL){
			if (temphE->pid == pide){
				find = 1;
				printk(KERN_INFO "hashentry FIND in while loop!!!");
				he = temphE;
				break;
			}else{
				if(temphE->next != NULL){
					temphE = temphE->next;
				}
				else{
					printk(KERN_INFO "***Alread traverse hashtable. The mailbox is not exist.");
					break;
				}
			}
		}

		if (find==1){
			printk(KERN_INFO "Mailbox FIND");
			ahashentry = he;
		}else{
			printk(KERN_INFO "createMailbox: 222222222222");
			ahashentry = createHashEntry(pide);
			temphE->next = ahashentry;
		}
	}
	printk(KERN_INFO "getEntry: end");
	spin_unlock(&ht->main_lock);
	return ahashentry;
}
int insertMsg(pid_t dest, void *msg, int len, bool block){
	HashEntry *he;
	mailbox *mb;
	message *newMsg;
	//	char* mmm = (char*) msg;

	printk(KERN_INFO "*************************** insertMsg *****************************\n");
	//	printk(KERN_INFO "dest:%d\n", dest);
	//	printk(KERN_INFO "len:%d\n", len);
	//	printk(KERN_INFO "msg:%s\n", mmm);
	he = getEntry(dest);
	printk(KERN_INFO "*************************** Success *****************************\n");

	mb = he->mb;

	spin_lock(&((&(he->wq))->lock));

	// Check message length
	if(len > MAX_MSG_SIZE){
		spin_unlock(&((&(he->wq))->lock));
		return MSG_LENGTH_ERROR;
	}

	if(((mb->msgNum) >= MAX_MAILBOX_MSG_NUM) && block == false){
		spin_unlock(&((&(he->wq))->lock));
		return MAILBOX_FULL;
	}

	// TODO: Add wait
	if(mb->msgNum >= MAX_MAILBOX_MSG_NUM && block == true){
		mb->ref_counter++;
		wait_event_interruptible_exclusive_locked_irq(mb->write_queue, mb->msgNum < MAX_MAILBOX_MSG_NUM);
		if (mb == NULL){
			return MAILBOX_INVALID;
		}


		printk(KERN_INFO "SendMsg: Process woken up");
		mb->ref_counter--;
	}

	newMsg = kmem_cache_alloc(message_cache, GFP_KERNEL);
	//	printk(KERN_INFO "*newMsg alloc success\n");
	//	printk(KERN_INFO "len:%d\n",len);
	newMsg->len = len;
	newMsg->sender = current->pid;
	//	printk(KERN_INFO "newMasg->len:\t%d\nnewMsg->sender:\t%d\n",newMsg->len, newMsg->sender);
	copy_from_user(newMsg->msg, msg, len);
	//	printk(KERN_INFO "*******newMsg->msg:%s********",newMsg->msg);
	mb->messages[mb->msgNum] = newMsg;
	mb->msgNum++;

	printk(KERN_INFO "SendMsg: %d messages in this mailbox\n", mb->msgNum);
	printk(KERN_INFO "SendMsg: Mailbox PID = %d\n", mb->pid);
	printk(KERN_INFO "SendMsg: New message = %s\n", mb->messages[mb->msgNum-1]->msg);
	printk(KERN_INFO "SendMsg: Message length = %d", mb->messages[mb->msgNum-1]->len);
	printk(KERN_INFO "*******************************************************************");

	if(mb->msgNum == 1 && mb->ref_counter > 0){
		wake_up_interruptible(&mb->read_queue);
	}

	spin_unlock(&((&(he->wq))->lock));

	return 0;
} // int insertMsg(int dest, char *msg, int len, bool block)

int removeMsg(pid_t *sender, void *msg, int *len, bool block){
	int i;
	pid_t currentpid = current->pid;
	HashEntry *he = getEntry(currentpid);
	mailbox *mb = he->mb;
	message *newMsg = NULL;

	printk(KERN_INFO "*************************** removeMsg *****************************\n");
	printk(KERN_INFO "hashEntryPID: %d\n",he->pid);
	printk(KERN_INFO "mailboxPID: %d\n",mb->pid);
	printk(KERN_INFO "mailboxMSGNUM: %d\n",mb->msgNum);
	printk(KERN_INFO "mailboxSTOP: %d\n",mb->stopped);
	//TODO: revise here
	spin_lock(&((&(he->wq))->lock));

	printk("RcvMsg: Mailbox PID = %d\n", mb->pid);
	printk("RcvMsg: There is %d messages in Mailbox\n", mb->msgNum);
	newMsg = mb->messages[0];
	if(newMsg == NULL){
		printk(KERN_INFO "RcvMsg: Mailbox is empty. Returning...");
		spin_unlock(&((&(he->wq))->lock));
		return -1;
	}

	// TODO: Add wait
	if(mb->msgNum == 0 && mb->stopped == false && block){
		mb->ref_counter++;
		wait_event_interruptible_exclusive_locked_irq(mb->read_queue, mb->msgNum > 0);
		if (mb == NULL){
			return MAILBOX_INVALID;
		}
		printk(KERN_INFO "RcvMsg: Process woken up");
		mb->ref_counter--;
	}

	if(mb->msgNum == 0 && mb->stopped == false && !block){
		spin_unlock(&((&(he->wq))->lock));
		return MAILBOX_EMPTY;
	}

	if(mb->stopped){
		if(mb->msgNum == 0){
			spin_unlock(&((&(he->wq))->lock));
			return MAILBOX_STOPPED;
		}

		printk(KERN_INFO "RcvMsg: Message = %s\n", newMsg->msg);
		msg = newMsg->msg;
		len = (int *)newMsg->len;
	}else {
		printk(KERN_INFO "RcvMsg: Message = %s\n", newMsg->msg);
		printk(KERN_INFO "RcvMsg: First character is %c\n", newMsg->msg[0]);
		printk(KERN_INFO "RcvMsg: Message length = %d\n", newMsg->len);

		// Copy the string back to the receiving mailbox
		if(copy_to_user(msg, newMsg->msg, newMsg->len)){
			spin_unlock(&((&(he->wq))->lock));
			return EFAULT;
		}

		// Copy sender PID
		if(copy_to_user(sender, &newMsg->sender, sizeof(pid_t))){
			spin_unlock(&((&(he->wq))->lock));
			return EFAULT;
		}

		// Copy message length
		if(copy_to_user(len, &newMsg->len, sizeof(int))){
			spin_unlock(&((&(he->wq))->lock));
			return EFAULT;
		}
	}

	if(mb->msgNum >= MAX_MAILBOX_MSG_NUM && mb->ref_counter > 0){
		wake_up_interruptible(&mb->write_queue);
		//		wake_up(&mb->write_queue);
	}

	// Update the array of messages inside the mailbox
	//	mb->msgNum--;
	//	for(i = 0; i < mb->msgNum; i++){
	//		mb->messages[i] = mb->messages[i + 1];
	//	}
	//	mb->messages[mb->msgNum] = NULL;

	for(i = 0; i < mb->msgNum; i++){
		if(i == mb->msgNum - 1){
			mb->messages[i] = NULL;
		}

		else{
			mb->messages[i] = mb->messages[i + 1];
		}
	}

	printk(KERN_INFO "*******************************************************************");
	spin_unlock(&((&(he->wq))->lock));
	return 0;
} // int removeMsg(int *sender, void *msg, int *len, bool block)

int remove(pid_t pid){
	int j;
	HashEntry *prev;
	HashEntry *crnt;
	mailbox* mb;

	spin_lock(&ht->main_lock);

	// Search for mailbox
	prev = ht->hE[hashfunc(pid)];
	crnt = ht->hE[hashfunc(pid)];

	while(crnt != NULL){
		if(crnt->pid == pid){
			// Reposition the linkedlist of hashEntrys
			prev->next = crnt->next;

			// Free up all messages
			mb = crnt->mb;
			for(j = 0; j < mb->msgNum; j++){
				kmem_cache_free(message_cache, &mb->messages[j]);
			}
			kfree(mb);

			spin_unlock(&ht->main_lock);
			return 0;
		}else{
			prev = crnt;
			crnt = crnt->next;
		}
	}

	spin_unlock(&ht->main_lock);
	return MAILBOX_INVALID; // Mailbox not found in hashtable

} // int remove(hashtable *h, int pid)

static unsigned long **find_sys_call_table(void) {
	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;

	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;

		if (sct[__NR_close] == (unsigned long *) sys_close) {
			printk(KERN_INFO "Interceptor: Found syscall table at address: 0x%02lX\n", (unsigned long) sct);
			return sct;
		}

		offset += sizeof(void *);
	}

	return NULL;
}	// static unsigned long **find_sys_call_table(void)

static void disable_page_protection(void) {
	/*
Control Register 0 (cr0) governs how the CPU operates.

Bit #16, if set, prevents the CPU from writing to memory marked as
read only. Well, our system call table meets that description.
But, we can simply turn off this bit in cr0 to allow us to make
changes. We read in the current value of the register (32 or 64
bits wide), and AND that with a value where all bits are 0 except
the 16th bit (using a negation operation), causing the write_cr0
value to have the 16th bit cleared (with all other bits staying
the same. We will thus be able to write to the protected memory.

It's good to be the kernel!
	 */

	write_cr0 (read_cr0 () & (~ 0x10000));
}	//static void disable_page_protection(void)

static void enable_page_protection(void) {
	/*
See the above description for cr0. Here, we use an OR to set the
16th bit to re-enable write protection on the CPU.
	 */

	write_cr0 (read_cr0 () | 0x10000);
}	// static void enable_page_protection(void)

static int __init interceptor_start(void) {
	/* Find the system call table */
	if(!(sys_call_table = find_sys_call_table())) {
		/* Well, that didn't work.
Cancel the module loading step. */
		return -1;
	}

	/* Store a copy of all the existing functions */
	ref_cs3013_syscall1 = (void *)sys_call_table[__NR_cs3013_syscall1];
	ref_cs3013_syscall2 = (void *)sys_call_table[__NR_cs3013_syscall2];
	ref_cs3013_syscall3 = (void *)sys_call_table[__NR_cs3013_syscall3];
	ref_sys_exit = (void *)sys_call_table[__NR_exit];
	ref_sys_exit_group = (void *)sys_call_table[__NR_exit_group];

	/* Intercept call */
	disable_page_protection();
	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)SendMsg;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)RcvMsg;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ManageMailbox;
	//sys_call_table[__NR_exit] = (unsigned long *)MailboxExit;
	//sys_call_table[__NR_exit_group] = (unsigned long *)MailboxExitGroup;
	enable_page_protection();

	//	spin_lock_init(&ht->main_lock);
	message_cache = kmem_cache_create("message_cache", sizeof(message), 0, 0, NULL);
	mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
	create();

	/* And indicate the load was successful */
	printk(KERN_INFO "Loaded interceptor!");

	return 0;
}	// static int __init interceptor_start(void)

static void __exit interceptor_end(void) {
	/* If we don't know what the syscall table is, don't bother. */
	if(!sys_call_table)
		return ;

	kfree(ht);

	kmem_cache_destroy(message_cache);

	/* Revert all system calls to what they were before we began. */
	disable_page_protection();
	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)ref_cs3013_syscall1;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)ref_cs3013_syscall2;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ref_cs3013_syscall3;
	//sys_call_table[__NR_exit] = (unsigned long *)ref_sys_exit;
	//sys_call_table[__NR_exit_group] = (unsigned long *)ref_sys_exit_group;
	enable_page_protection();


	printk(KERN_INFO "Unloaded interceptor!");
}	// static void __exit interceptor_end(void)

module_init(interceptor_start);
module_exit(interceptor_end);
