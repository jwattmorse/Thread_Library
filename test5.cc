// Test if libin not called first


#include <cassert>
#include <iostream>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string>
#include <fstream>
#include "thread.h"
#include <queue>
#include "interrupt.h"
#include <map>
#include <sys/resource.h>


using namespace std;


void hello (void* arg){

  cout << "hello" << endl;
}



void lotsOfLocks(void* arg){
  int x;
  for( x =1; x < 100000; x++){
    if (thread_lock(x) == -1)
      cout << "success?" << endl;
    thread_unlock(x);
    
  }
  //if (){
  //perror("too many looks");
  //}
}


void testControler(void* arg){
  
  thread_create((thread_startfunc_t) lotsOfLocks, NULL);
  //thread_create((thread_startfunc_t) lotsOfLocks, NULL);
  //thread_create((thread_startfunc_t) lotsOfLocks, NULL);

  thread_create((thread_startfunc_t) hello, NULL);
  
  
}




void badStart(void *arg){

  perror("Failed Test");
  exit(1);
}


int main(){
  
  


  struct rlimit lim;
  lim.rlim_cur = lim.rlim_max = 10*1024*1024; /* 10 MB */

  if (setrlimit(RLIMIT_AS, &lim)) {
    perror("setrlimit");
    exit(1);
  }


  thread_create((thread_startfunc_t) badStart, NULL);
  cout << "Success!" << endl;
  
  thread_libinit((thread_startfunc_t) testControler, NULL);

  return 0;
}
