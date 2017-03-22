#include "thread.h"
#include <iostream> 
#include <stdio.h>
#include <sys/resource.h>
//Mai Toms and Jacob Watt-Morse
//This class limits the mememory and then tries to create threads.
//Expected output: 
//
 /*
hi
Control begin
Thread 1 begin
.
.
.
Thread 36 begin
Thread 37 begin
Thread 38 begin
Thread 39 begin
Thread 40 begin
Thread library exiting.

  */

using namespace std;

void thread(void* arg){
  cout << "Thread "<< (intptr_t)arg <<" begin" << endl;
}

void tooManyThreads(void* arg){
  for(intptr_t x =1; x < 300; x++){
    thread_create((thread_startfunc_t) thread, (void*) x);
  }
}

int main(int argc, char** argv) {
  cout << "hi" << endl;
  struct rlimit lim;
  lim.rlim_cur = lim.rlim_max = 100 *STACK_SIZE; /* 10 MB */
  if (setrlimit(RLIMIT_AS, &lim)) {
    perror("setrlimit");
    exit(1);
  }
  thread_libinit( (thread_startfunc_t) tooManyThreads, NULL);
 
  return 0;
}
