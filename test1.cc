#include "thread.h"
#include <iostream> 
#include <stdio.h>
//Mai Toms and Jacob Watt-Morse
//This class tests thread_libinit, thread_create, and thread_yield
//It does not test any locks
//Expected output:
/*
hi
Thread 1 begin
Thread 2 begin
Thread 2 end
Thread 1 end
Thread library exiting.
*/
using namespace std;

void thread1(void* arg){
  cout << "Thread 1 begin" << endl;
  thread_yield();
  thread_yield();
  cout << "Thread 1 end" << endl;
}

void thread2(void* arg){
  cout << "Thread 2 begin" << endl;
  cout << "Thread 2 end" << endl;
  thread_yield();
  thread_yield();
  thread_yield();
  thread_yield();
  thread_yield();
}

void control(void* arg){
  thread_create((thread_startfunc_t) thread1, NULL);
  thread_create((thread_startfunc_t) thread2, NULL);
}


int main(int argc, char** argv) {
  cout << "hi" << endl;
  thread_libinit( (thread_startfunc_t) control, NULL);
 
  return 0;
}
