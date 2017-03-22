// Jacob Watt-Morse and Mai Toms
// (c) 2017


// TO DO:
// Each function needs an assertion if LIBININ has not been called
// Helper Method to handle actually starting a thread.
// Figure out thread create

/*
 * Implmentation Idea:
 * Creating a thread creaets a level of misdirection by creating a helper thread 
 * that takes the function as a parameter and runs the function the user thought
 * was running as an indepedent thread.
 * whenver a thread terminates or is blocked we check to see if there are items 
 * left on queue and print thread termingate message if queue is empty 
 */

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

using namespace std;

//structure to represent a lock
struct lock_str {
  ucontext_t* owner;
  queue<ucontext_t*>* waiting;
};


static queue<ucontext_t *> runningQueue; //maintains list of runnable threads
static ucontext_t* cur; //current thread
static ucontext_t* old_thread; //thread that most recently finished running

static bool initlib = false; //keeps track of whether or not libinit was called

//map of locks, represented by ints, to a lock structure
static map<unsigned int, lock_str*> lockMap;
static map<unsigned int, lock_str*>::iterator iter;

//map of condition variables to a queue of waiting threads
static map<string, queue<ucontext_t*>* > cvMap; 
static map<string, queue<ucontext_t*>* >::iterator cvIter; 

//Threadhelpper function runs as its own thread that calls the function
//we wanted to run as a thread. Cleans up memory from thread that previously ran
static void threadHelper(thread_startfunc_t func, void *arg){
  interrupt_enable();
  
  //run the given function
  func(arg);

  interrupt_disable();

  // Delete previous thread if it exists
  if(old_thread != NULL){
    delete[] (char *) old_thread->uc_stack.ss_sp;
    free(old_thread); 
  }
  old_thread = cur;

  //check if there is a new thread to be run 
  //if not, exit the library
  if ( runningQueue.empty()){
    cout << "Thread library exiting." << endl;
    // clean up memory?
  } else {
    cur = runningQueue.front();
    runningQueue.pop();
    swapcontext(old_thread, cur);    
  }
  interrupt_enable();
}



int thread_create(thread_startfunc_t func, void *arg){
  if ( !initlib)
    return -1; // check that thread library was initialized
  
  interrupt_disable();
  
  ucontext_t* ucontext_ptr;
  try {
    //get current context 
    ucontext_t* ucontext_ptr =(ucontext_t*)malloc(sizeof(ucontext_t));
    getcontext(ucontext_ptr);    // ucontext_ptr has type (ucontext_t *)

    char *stack = new char [STACK_SIZE];
    ucontext_ptr->uc_stack.ss_sp = stack;
    ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
    ucontext_ptr->uc_stack.ss_flags = 0;
    ucontext_ptr->uc_link = NULL;

    //make new context and add to runnable queue
    runningQueue.push(ucontext_ptr);
    makecontext(ucontext_ptr, (void(*)()) threadHelper, 2, func, arg);
    } catch (bad_alloc) { 
    //deal with memory
    delete[] (char *) ucontext_ptr->uc_stack.ss_sp;
    free(ucontext_ptr); 

    interrupt_enable();
    return -1;
    }
  interrupt_enable();
  
}

int thread_lock(unsigned int lock){
  // if lock quene does not exists: create a lock queue
  // need a dictionary of lock queues
  if ( !initlib)
    return -1;

  interrupt_disable();
  //search for lock in list of current locks
  lock_str* curLock;
  iter = lockMap.find(lock);
  //no lock found. create one and add to map
  if(iter == lockMap.end()){
      try{
      curLock = new lock_str; 
      curLock->owner = cur;
      curLock->waiting =  new queue<ucontext_t*>; 
    }catch (bad_alloc b){
      delete curLock; //if too many locks have been make return error
      interrupt_enable();
      return -1;
    }
    lockMap.emplace(lock, curLock); //add to map
  } else {
    //lock found
    curLock = iter->second;
    //acquire lock if it is free
    if ( curLock->owner == NULL ){
      curLock->owner = cur;
    } else {
      //if thread already owns lock return error
      if (cur == curLock->owner){
	interrupt_enable();
	return -1;
      } 
      
      //try to move to a new context if lock is held by another thread
      if ( runningQueue.empty()){
	cout << "Thread library exiting." << endl;
	exit(1);
      }
      
      ucontext_t* old = cur; //save context
      curLock->waiting->push(old);
      cur = runningQueue.front(); //get next runnable thread
      runningQueue.pop();    
      swapcontext(old, cur);  //switch into next context
    }
  }
  interrupt_enable();
}
  

  
int unlock_helper(unsigned int lock){
  
  iter = lockMap.find(lock); //find lock
  //no lock found. return error.
  if(iter == lockMap.end()) 
    return -1;
  lock_str* curLock = iter->second; //retrieve lock
  //lock is free or current thread isn't owner. return error
  if (( curLock->owner == NULL) || (curLock->owner != cur ))
    return -1;

  //if there are no threads waiting just set thread as unlocked
  if(curLock->waiting->empty() ) {
    curLock -> owner = NULL;
  } else{
    //get next waiting thread and add to running queue
    curLock -> owner = curLock -> waiting-> front();
    curLock -> waiting-> pop();
    runningQueue.push(curLock->owner);
  }
  
  return 0;
}


int thread_unlock(unsigned int lock){
  if ( !initlib)
    return -1;

  interrupt_disable();
  int result = unlock_helper(lock);
  interrupt_enable();
  
  return result;
}


//pre: interrupts disabled
//post: next thread is pulled off of runningQueue or thread lib exits
int run_next_thread(){
   // should print out terminating message if no more threads can be run
  if ( runningQueue.empty()){
    cout << "Thread library exiting." << endl;
  }else{
    ucontext_t* old = cur; //save context
    cur = runningQueue.front(); //get next runnable thread
    runningQueue.pop();    
    swapcontext(old, cur);  //switch into next context
  }
  return 1;
}

int thread_wait(unsigned int lock, unsigned int cond){
  if ( !initlib)
    return -1;
  interrupt_disable();
  //unlock failed, return error
  if(unlock_helper(lock)==-1)
    return -1;

  string key = lock + " " + cond;
  cvIter = cvMap.find(key);
  //add new CV
  if(cvIter == cvMap.end()){
    //add try catch?
    queue<ucontext_t*>* cvWaiting;
    try{
      cvWaiting = new queue<ucontext_t*>;
    } catch(bad_alloc b){
      delete cvWaiting; //if too many CVs delete
      interrupt_enable();
      return -1;
    }
    cvWaiting->push(cur);
    cvMap.insert(make_pair(key, cvWaiting));
  } else{
    //CV found. add to queue.
    cvIter->second->push(cur);
  }
  run_next_thread(); //is this method helpful?
  interrupt_enable();
  return thread_lock(lock); // do we need onther helper?
}


int thread_signal(unsigned int lock, unsigned int cond){
  if ( !initlib)
    return -1;
  interrupt_disable();
  //search for lock/cond combo in map
  string key = lock + " " + cond;
  cvIter = cvMap.find(key);
  //
  if(cvIter != cvMap.end()){
    queue<ucontext_t*>* cvWaiting = cvIter->second;
    if(!cvWaiting->empty()){
      ucontext_t* new_thread = cvWaiting->front();
      cvWaiting->pop();
      runningQueue.push(new_thread);
    }
  }
  interrupt_enable();
  return 0;
}


int thread_broadcast(unsigned int lock, unsigned int cond){
  if ( !initlib)
    return -1;
  interrupt_disable();
  //search for lock/cond combo in map
  string key = lock + " " + cond;
  cvIter = cvMap.find(key);
  if(cvIter != cvMap.end()){
    queue<ucontext_t*>* cvWaiting = cvIter->second;
    while(cvWaiting->size()>0){
      ucontext_t* new_thread = cvWaiting->front();
      cvWaiting->pop();
      runningQueue.push(new_thread);
    }
  }
  interrupt_enable();
  return 0;
}


/* thread_yield causes the current thread to yield the CPU to the next
*  runnable thread.
*/
int thread_yield(void){
  
  interrupt_disable();

  ucontext_t* old = cur; //save context that called yield
  if(!runningQueue.empty()){
      cur = runningQueue.front(); //get next runnable thread
      runningQueue.pop();
      runningQueue.push(old); //add context that called yield back to queue
      swapcontext(old, cur);  //switch into next context
    }
  interrupt_enable();

}



/*
 * libinit should work as a controller function and print the end statement
 * should use a helper function to call the new thread
 */
int thread_libinit(thread_startfunc_t func, void *arg){
  //libinit should only be called once
  if(libinit)
    return -1;

  // disable interupts;
  interrupt_disable();

  initlib = true;
  
  //why can't we say new??
  //  ucontext_t* ucontext_ptr = new ucontext_t;
  //get current context
  ucontext_t* ucontext_ptr = (ucontext_t*)malloc(sizeof(ucontext_t)); 
  getcontext(ucontext_ptr);                                                                      
  char *stack = new char [STACK_SIZE];
  ucontext_ptr->uc_stack.ss_sp = stack;
  ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
  ucontext_ptr->uc_stack.ss_flags = 0;
  ucontext_ptr->uc_link = NULL;

  cur = ucontext_ptr;
  //make new context and immediately switch into it
  makecontext(ucontext_ptr, (void(*)()) threadHelper, 2, func, arg);
  setcontext(ucontext_ptr); 

  //enable interupts; // do we need this
  interrupt_enable();
}
