#include "thread.h"
#include <iostream> 
#include <stdio.h>
//Mai Toms and Jacob Watt-Morse
//This class tests error checking in the thread library.
// note: we output the line "thread_library not initialized\n" 
int lock1 = 1;
int lock2 = 2;
int no_lock = 3;

int no_cond = 4;
using namespace std;

void thread(void* arg){
  thread_lock(lock2);
}

void error_checks( void* arg){
  thread_lock(lock1);
  if (thread_lock(lock1) == 0)
    cout << "acquired lock already holding the lock\n";
  if (thread_unlock(lock2) == 0)
    cout << "unlocked lock owned by other thread";
  if (thread_wait(no_lock, no_cond) == 0)
    cout << "waiting on a lock that doesn't exist\n";
  if (thread_wait(lock2, no_cond) == 0)
    cout << "waiting on a lock owned by other thread\n";
  if (thread_signal(no_lock, no_cond)!=0)
    cout << "incorrect handling on non-existent condition\n";
}


void control(void* arg){
  if(thread_libinit( (thread_startfunc_t) control, NULL)==0)
    cout << "called init lib twice\n";
  thread_create((thread_startfunc_t) thread, NULL);
  thread_create((thread_startfunc_t) error_checks, NULL);
}


int main(int argc, char** argv) {
  cout << "hi" << endl;
  if (thread_create((thread_startfunc_t) thread, NULL)==0)
     cout << "ran thread before calling init lib\n";
  thread_libinit( (thread_startfunc_t) control, NULL);
 
  return 0;
}
