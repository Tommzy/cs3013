
#undef __KERNEL__
#undef MODULE

#define __KERNEL__
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/cred.h>
#include "mailbox.h"
unsigned long **sys_call_table;
asmlinkage long (*ref_sys_cs3013_syscall1)(void);
asmlinkage long (*ref_sys_cs3013_syscall2)(void);
asmlinkage long (*ref_sys_cs3013_syscall3)(void);
	  

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	CS3013_message *message;
	int downReturn;
	struct task_struct *Dest = find_task_by_vpid(dest);

	// make sure we can get the task_struct by PID and that it has a mailbox
	if(!Dest || Dest == NULL || Dest->mailbox == NULL) return MAILBOX_INVALID;
	// bail right away if the mailbox is stopped
	if(Dest->mailbox->stopped) return MAILBOX_STOPPED;
	// ignore messages that are too long
	if(len > MAX_MSG_SIZE) return MSG_TOO_LONG;
	
	// reserve a slot in our CS3013_message slab
	message = alloc_CS3013_message();
	// populate the message data structure
	if(copy_from_user(&message->text, msg, len))	return MSG_ARG_ERROR;
	message->length = len;
	message->sender_pid = task_pid_nr(current);
	INIT_LIST_HEAD(&message->list);

	if(block == BLOCK) {
		// keep track of the number waiting for cleanup during exit
		++Dest->mailbox->waitingSenders;
		downReturn = down_interruptible(Dest->mailbox->empty);
		--Dest->mailbox->waitingSenders;
		// if we were interrupted, retry
		if(downReturn) return -ERESTARTSYS;
	} else {
		// only "try" to lock so we don't block
		if(down_trylock(Dest->mailbox->empty))
		return MAILBOX_FULL;
	}
	// if we were signaled while stopped, quit here
	if(Dest->mailbox->stopped) {
		up(Dest->mailbox->empty);
		return MAILBOX_STOPPED;
	}

	// get the spinlock so we can modify the messages list
	spin_lock(&Dest->mailbox_lock);
	// if we were signaled while stopped, quit here
	if(Dest->mailbox->stopped) {
		spin_unlock(&Dest->mailbox_lock);
		up(Dest->mailbox->empty);
		return MAILBOX_STOPPED;
	}
	// either start a new list or append our message to the list
	if(Dest->mailbox->messages == NULL) {
		Dest->mailbox->messages = &message->list;
	} else {
		list_add_tail(&message->list, Dest->mailbox->messages);
	}

	// release our locks
	up(Dest->mailbox->full);
	spin_unlock(&Dest->mailbox_lock);

	return 0;
}


asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	CS3013_message *message;
	int downReturn;
	struct task_struct *self = current;

	// make sure we have a mailbox
	if(self->mailbox == NULL) return MAILBOX_INVALID;
	// ... and that it's not stopped
	if(self->mailbox->stopped) return MAILBOX_STOPPED;

	// same few operations as in mailbox_send
	if(block == BLOCK) {
		++self->mailbox->waitingReceivers;
		downReturn = down_interruptible(self->mailbox->full);
		--self->mailbox->waitingReceivers;
		if(downReturn) return -ERESTARTSYS;
	} else {
		if(down_trylock(self->mailbox->full))
		return MAILBOX_EMPTY;
	}
	if(self->mailbox->stopped) {
		up(self->mailbox->full);
		return MAILBOX_STOPPED;
	}

	spin_lock(&self->mailbox_lock);

	if(self->mailbox->stopped) {
		spin_unlock(&self->mailbox_lock);
		up(self->mailbox->full);
		return MAILBOX_STOPPED;
	}

	// just a sanity check: the messages list should not be empty at this point
	if(self->mailbox->messages == NULL) {
		spin_unlock(&self->mailbox_lock);
		return MAILBOX_EMPTY;
	}

	// get a reference to the first message in the list
	message = list_entry(self->mailbox->messages, CS3013_message, list);
	// if this is the last message, reset the list to NULL (empty)
	if(message->list.next == &message->list) self->mailbox->messages = NULL;
		// otherwise just point to the next message
		else self->mailbox->messages = message->list.next;
	// re-link the adjacent members
	list_del(&message->list);

	// release the locks
	up(self->mailbox->empty);
	spin_unlock(&self->mailbox_lock);

	// copy the message, sender, and length to the user-supplied locations
	if(msg != NULL)	copy_to_user(&message->text, msg, message->length);
	if(sender_pid != NULL)	copy_to_user(&message->sender_pid, sender_pid, sizeof(pid_t));
	if(len != NULL)	copy_to_user(&message->length, len, sizeof(int));

	// free the message
	free_CS3013_message(message);

	return 0;
}

asmlinkage long ManageMailbox(bool stop, int *count){
	struct task_struct *self = current;
	struct list_head *tmp;
	int count = 0;

	// make sure we have a mailbox
	if(self->mailbox == NULL) return MAILBOX_INVALID;
	// stop the mailbox if requested
	if(stop) self->mailbox->stopped = true;

	// if the list is non-empty, count the number of messages
	if(self->mailbox->messages != NULL) {
		// start at one, since the first element isn't enumerated
		++count;
		// lock while counting
		spin_lock(&self->mailbox_lock);
		 __list_for_each(tmp, self->mailbox->messages) {
			++count;
		}
		spin_unlock(&self->mailbox_lock);
	}

	// copy the count back to the user
	if(usr_count != NULL) copy_to_user(&count, usr_count, sizeof(int));

	return 0;
}

static unsigned long **find_sys_call_table(void) {
	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;

	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;

		if (sct[__NR_close] == (unsigned long *) sys_close) {
			printk(KERN_INFO "Interceptor: Found syscall table at address: 0x%02lX", (unsigned long) sct);
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
	ref_sys_cs3013_syscall1 = (void *)sys_call_table[__NR_cs3013_syscall1];
	ref_sys_cs3013_syscall2 = (void *)sys_call_table[__NR_cs3013_syscall2];
	ref_sys_cs3013_syscall3 = (void *)sys_call_table[__NR_cs3013_syscall3];
	/* Replace the existing system calls */
	disable_page_protection();

	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)SendMsg;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)RcvMsg;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ManageMailbox;

	enable_page_protection();

	/* And indicate the load was successful */
	printk(KERN_INFO "Loaded interceptor!");

	return 0;
}	// static int __init interceptor_start(void)


static void __exit interceptor_end(void) {
	/* If we don't know what the syscall table is, don't bother. */
	if(!sys_call_table)
		return;

	/* Revert all system calls to what they were before we began. */
	disable_page_protection();

	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)ref_sys_cs3013_syscall1;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)ref_sys_cs3013_syscall2;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ref_sys_cs3013_syscall3;

	enable_page_protection();

	printk(KERN_INFO "Unloaded interceptor!");
}	// static void __exit interceptor_end(void)

MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);

