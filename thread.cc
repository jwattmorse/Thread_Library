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

//pre: interrupts disabled
//post: next thread is pulled off of runningQueue or thread lib exits
static int run_next_thread(){
   // should print out terminating message if no more threads can be run
  if ( runningQueue.empty()){
    cout << "Thread library exiting." << endl;
    exit(0);
  }else{
    ucontext_t* old = cur; //save context
    cur = runningQueue.front(); //get next runnable thread
    runningQueue.pop();    
    swapcontext(old, cur);  //switch into next context
  }
  return 1;
}

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
    exit(0);
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
  
  ucontext_t* ucontext_ptr = NULL;
  char *stack = NULL;
  //try to get new context
  try {
    ucontext_ptr =(ucontext_t*)malloc(sizeof(ucontext_t));
  } catch (bad_alloc) { 
    //delete and return error if malloc fails
    free(ucontext_ptr); 
    interrupt_enable();
    return -1;
  }
  getcontext(ucontext_ptr); 
  try{
    stack = new char [STACK_SIZE];
    ucontext_ptr->uc_stack.ss_sp = stack;
    ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
    ucontext_ptr->uc_stack.ss_flags = 0;
    ucontext_ptr->uc_link = NULL;
    //make new context and add to runnable queue
    runningQueue.push(ucontext_ptr);
    makecontext(ucontext_ptr, (void(*)()) threadHelper, 2, func, arg);
  } catch (bad_alloc) { 
    //delete and return error if creating a new stack fails
    delete[] stack;
    free(ucontext_ptr); 
    interrupt_enable();
    return -1;
    }
  interrupt_enable();
  
}
//pre: interrupts disabled and thread libriary initialized
static int lock_helper(unsigned int lock){
  
  //search for lock in list of current locks
  lock_str* curLock;
  iter = lockMap.find(lock);

  //no lock found. create one and add to map
  if(iter == lockMap.end()){
    try{
      curLock = new lock_str; 
    }catch (bad_alloc b){
       delete curLock;
       return -1;
    }
    try{
      curLock->owner = cur;
      curLock->waiting =  new queue<ucontext_t*>; 
    }catch (bad_alloc b){
      delete curLock->waiting;
      delete curLock; //if too many locks have been make return error
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
	return -1;
      } 
      
      //try to move to a new context if lock is held by another thread
      if ( runningQueue.empty()){
	cout << "Thread library exiting." << endl;
	exit(0);
      }
      
      ucontext_t* old = cur; //save context
      curLock->waiting->push(old);
      cur = runningQueue.front(); //get next runnable thread
      runningQueue.pop();    
      swapcontext(old, cur);  //switch into next context
    }
  }
  return 0;
}

int thread_lock(unsigned int lock){
  if ( !initlib)
    return -1;
  interrupt_disable();
  int result = lock_helper(lock);
  interrupt_enable();
  return result;
}

//pre: thread lib initialized and interrupts disabled 
//post: if a valid thread called unlock, lock is set as free or
// next thread waiting for lock is added to running queue 
static int unlock_helper(unsigned int lock){
  
  iter = lockMap.find(lock); //find lock
  //no lock found. return error.
  if(iter == lockMap.end()) 
    return -1;
  lock_str* curLock = iter->second; //retrieve lock
  //lock is free or current thread isn't owner. return error
  if (curLock->owner != cur )
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

int thread_wait(unsigned int lock, unsigned int cond){
  if ( !initlib)
    return -1;
  interrupt_disable();
  //unlock failed, return error
  if(unlock_helper(lock)==-1){
    interrupt_enable();
    return -1;
  }
  //find unique lock, condition variable combo in map
  string key = lock + " " + cond;
  cvIter = cvMap.find(key);
  //add new CV
  if(cvIter == cvMap.end()){
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
  run_next_thread();
  int result = lock_helper(lock); // do we need onther helper?
  interrupt_enable();
  return result;

}


int thread_signal(unsigned int lock, unsigned int cond){
  if ( !initlib)
    return -1;
  interrupt_disable();
  //search for lock/cond combo in map
  string key = lock + " " + cond;
  cvIter = cvMap.find(key);
  //if a thread is waiting add it to the running queue
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
  //add all waiting threads to running queue
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
*  runnable thread. If there are no other runnable threads nothing changes
*/
int thread_yield(void){
  if ( !initlib)
    return -1;

  interrupt_disable();
  if(!runningQueue.empty()){
    ucontext_t* old = cur; //save context that called yield
    cur = runningQueue.front(); //get next runnable thread
    runningQueue.pop();
    runningQueue.push(old); //add context that called yield back to queue
    swapcontext(old, cur);  //switch into next context
    }
  interrupt_enable();
  return 0;
}



/*
 * libinit runs the first thread. Should be called first and only once/ 
 */
int thread_libinit(thread_startfunc_t func, void *arg){
  //chech that libinit is only called once
  if(initlib){
    cout<< "thread_libinit failed\n" <<endl; //our program doesn't work with out this line and we don't know why.....
    return -1;
  }
  
  // disable interupts;
  interrupt_disable();

  initlib = true;
  
  //create new context 
  ucontext_t* ucontext_ptr;
  try {
    ucontext_ptr = (ucontext_t*)malloc(sizeof(ucontext_t)); 
    getcontext(ucontext_ptr);                                                   
    char *stack = new char [STACK_SIZE];
    ucontext_ptr->uc_stack.ss_sp = stack;
    ucontext_ptr->uc_stack.ss_size = STACK_SIZE;
    ucontext_ptr->uc_stack.ss_flags = 0;
    ucontext_ptr->uc_link = NULL;
  }catch (bad_alloc){   
    delete[] (char*) ucontext_ptr->uc_stack.ss_sp;
    free(ucontext_ptr);
    return -1;
  }
  cur = ucontext_ptr;
  //make new context and immediately switch into it
  makecontext(ucontext_ptr, (void(*)()) threadHelper, 2, func, arg);
  setcontext(ucontext_ptr); 
  interrupt_enable();
  return 0;
}
