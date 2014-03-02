#include "mailbox.h"
#include <stdio.h>

int main() {
  int childPID = fork();
  
  if(childPID == 0){
    pid_t sender;
    void *msg[128];
    int len;
    bool block = true;
    RcvMsg(&sender,msg,&len,block);
    printf("Message received.\n");
    printf("Message: %s\n", (char *) msg);
  }
  else{
    char mesg[] = "I am your father";
    printf("Sending Message to child.\n");
    if (SendMsg(childPID, mesg, 17, false)){
      printf("Send failed\n");
    }
  }
  return 0;
}
