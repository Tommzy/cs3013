/* Alfredo Porras
 * July 12th, 2011
 * CS 3013
 * Project 4 - test program 1
 * Tests if messages can be sent and received.
 */

#include "mailbox.h"
#include <stdio.h>
#include <unistd.h>

int main() {
  int childPID = fork();
  printf("fork success!.\n");
  
  if(childPID == 0){
    printf("In child.\n");
    pid_t sender;
    void *msg[128];
    int len;
    bool block = true;
    printf("prepare to receive.\n");
    RcvMsg(&sender,msg,&len,block);
              printf("receive success.\n");
    printf("Message received.\n");
    printf("Message: %s\n", (char *) msg);
  }
  else{
  printf("In father .\n");
    char mesg[] = "I am your father";
    printf("Sending Message to child.\n");
    if (SendMsg(childPID, mesg, 17, false)){
      printf("Send failed\n");
    }
        printf("Message send success.\n");
  }
  return 0;
}
