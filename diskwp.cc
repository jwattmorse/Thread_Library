//edit test
#include <fstream>
#include <string>
#include <stdio.h>
#include <iostream> 
#include <stdlib.h> //for abs()
#include <algorithm>   //for min
#include <deque> //maybe?
#include "thread.h"
using namespace std;

// failed test #3
// What is this test?
// Asyncronous premptions 
// Not holding lock when doing something

//structure that represents a request
struct Req {
  int id;
  int track;
};

int max_disk_queue, num_threads, cur_track, my_lock, active_threads;

//condition codes
int queue_full, queue_open;

//array of file names
char** file_names;

//disk queue representation
deque<Req*> disk_queue;  

//function to sort requests
bool sortRequests(Req* a, Req* b){
  return abs(a->track - cur_track) < abs(b->track - cur_track);
}

//checks if queue is full or maxed out based on num active threads
bool diskQueueFull(){
  return disk_queue.size()>= max_disk_queue;
}

//helper method to service request
//pre: disk_queue is not empty
//post: request at front of queue is serviced
void serviceRequest(){
  Req* cur_req = disk_queue.front();
  disk_queue.pop_front();
  cur_track =  cur_req->track;
  cout << "service requester " << cur_req->id << " track " << cur_req->track << endl;

  thread_signal(my_lock,cur_req->id);
  //need to somehow tell requestors that request was serviced?
}

//pre: new_req is a valid Req and disk_queue is not full
//post: disk_queue is sorted by distance to current track with new_req added
void sendRequest(Req* new_req){
  cout << "requester " << new_req->id << " track " << new_req->track << endl;
  //add req to queue and sort by distance to cur_track
  disk_queue.push_front(new_req);
  sort(disk_queue.begin(), disk_queue.end(), sortRequests);
}

void servicer(void* id){
  while ( active_threads > 0 ){
    thread_lock(my_lock);
    
    while(!diskQueueFull()){
    //cout <<" hi"<<endl;
      thread_wait(my_lock, queue_full);
    }
    if(!disk_queue.empty())
      serviceRequest();
    thread_broadcast(my_lock,queue_open); //do I need this?

    thread_unlock(my_lock);
  }
}

void requester(void* idptr){
  //cast pointer to ID
  int id = (intptr_t)idptr;
  //position of file in array
  int file_pos = id + 2;
   
  //open file
  ifstream stream (file_names[file_pos]);
  char command [512];
  
  while(stream >> command){
    //create new Req
    //cout << command << endl;
    Req* cur_req = new Req;
    cur_req->id = (int)id;
    cur_req->track = atoi(command);
    //cout << id << "about to aquire lock"<< endl;
    thread_lock(my_lock);
    while(diskQueueFull())
      thread_wait(my_lock,queue_open); //what else would I put here if don't need broadcast?

    sendRequest(cur_req);

    //if(diskQueueFull())
    thread_broadcast(my_lock,queue_full); //broadcast unlocks so is this wrong?
    
    thread_wait(my_lock, id); //wait until request is serviced to continue
    //cout << id << " I woke up"<< endl;
  }

  active_threads--;
  if (active_threads< max_disk_queue){
    max_disk_queue--;
    thread_broadcast(my_lock,queue_full);
  } 
  //broadcast unlocks so is this wrong?
  thread_unlock(my_lock);

    
}
  


//creates initial threads
void control(void* arg) {
  start_preemptions(true,false,900);
  active_threads = num_threads; 
  thread_create(servicer, NULL);

  for (intptr_t i = 0; i < num_threads; i++){
    //cout << i << " \n";
    thread_create((thread_startfunc_t) requester, (void*) i);
  }
}

int main(int argc, char** argv) {
  //parse input

  //set max disk size
  max_disk_queue = atoi(argv[1]);
  //set number of threads
  num_threads = argc -2;
  //save file names from arg array
  file_names = argv;
  //set current track position
  cur_track = 0;
  //create a lock for the the thread
  my_lock = 1;
  //set condition variables
  queue_full = num_threads+1;
  queue_open = num_threads+2;
  
  //cout << num_threads << " \n";
  thread_libinit( (thread_startfunc_t) control, NULL);
  return 0;
}
