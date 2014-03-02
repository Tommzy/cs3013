obj-m := Mailbox_LKM.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

testmailbox1: testmailbox1.o mailbox.o
	gcc $(CFLAGS) -lm testmailbox1.o mailbox.o -o testmailbox1

testmailbox1.o: testmailbox1.c
	gcc -Wall -c $(CFLAGS) testmailbox1.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
