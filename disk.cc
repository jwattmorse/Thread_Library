// Jacob Watt-Morse and Mai Toms
// 3/9/2017
// Disk Scheduler
#include <fstream>
#include <string>
#include <stdio.h>
#include <iostream> 
#include <stdlib.h> //for abs()
#include <algorithm>   //for min
#include <deque> //maybe?
#include "thread.h"
using namespace std;



//structure that represents a request
struct Req {
  int id;
  int track;
};

//globals for disk capacity, number of total and active threads and where the disk head is  
int max_disk_queue, num_threads, cur_track, active_threads; 

int my_lock; // lock for shared data


int queue_full, queue_open; //condition codes

char** file_names; //array of file names


deque<Req*> disk_queue;  //disk queue representation


//function to sort requests
bool sortRequests(Req* a, Req* b){
  return abs(a->track - cur_track) < abs(b->track - cur_track);
}

//checks if queue is full or maxed out based on # active threads
bool diskQueueFull(){
  return disk_queue.size()>= max_disk_queue;
}

//helper method to service request
//pre: disk_queue is not empty
//post: request at front of queue is serviced
void serviceRequest(){
  // get request to service
  Req* cur_req = disk_queue.front();
  disk_queue.pop_front();
  cur_track =  cur_req->track; //set cur_track to current read 
  
  //message of serviced thread
  cout << "service requester " << cur_req->id << " track " << cur_req->track << endl;
  
  // sort because head location changed
  if(!disk_queue.empty())
    sort(disk_queue.begin(), disk_queue.end(), sortRequests);
  
  thread_signal(my_lock,cur_req->id); // signal thread that was serviced
}



//pre: new_req is a valid Req and disk_queue is not full
//post: disk_queue is sorted by distance to current track with new_req added
void sendRequest(Req* new_req){
  //message that request was sent
  cout << "requester " << new_req->id << " track " << new_req->track << endl;
  
  disk_queue.push_front(new_req);
  sort(disk_queue.begin(), disk_queue.end(), sortRequests);
}


// thread that services the request
void servicer(void* id){
  while ( active_threads > 0 ){
    thread_lock(my_lock); //locks when servicing
    
    //wait until queue is full
    while(!diskQueueFull()){
      thread_wait(my_lock, queue_full);
    }
    
    //service request and let requesters know there is space in queue
    if(!disk_queue.empty())
      serviceRequest();
    thread_broadcast(my_lock,queue_open); 
    thread_unlock(my_lock);
  }
}


// Threads that request disk reads
void requester(void* idptr){
  //cast pointer to ID
  int id = (intptr_t)idptr;
 
  //position of file in array
  int file_pos = id + 2;
   
  //open file
  ifstream stream (file_names[file_pos]);
  char command [512];
  
  while(stream >> command){
    // create new requester object
    Req* cur_req = new Req;
    cur_req->id = (int)id;
    cur_req->track = atoi(command);
 
    thread_lock(my_lock); //lock when checking queue
    
    // wait until space in queue 
    while(diskQueueFull())
      thread_wait(my_lock,queue_open); 

    sendRequest(cur_req); 

    thread_broadcast(my_lock,queue_full); 
    thread_wait(my_lock, id); //wait until request is serviced to continue
  }
  
  active_threads--; //when done decrement number of active threads
  
  // if less threads then max size of queue
  // allow servicers to service with fewer elements in queue
  if (active_threads< max_disk_queue){
    max_disk_queue--;
    thread_broadcast(my_lock,queue_full);
  } 
  thread_unlock(my_lock); // unlock when done requesting objects
}

// creates each requester thread
void control(void* arg) {
  active_threads = num_threads; 
  thread_create(servicer, NULL);
 
  for (intptr_t i = 0; i < num_threads; i++){
    thread_create((thread_startfunc_t) requester, (void*) i);
  }
}


// main method parses input then calls controler to start scheduling
int main(int argc, char** argv) {
  
  max_disk_queue = atoi(argv[1]);  //set max disk size
  num_threads = argc -2; //set number of threads
  file_names = argv; //save file names from arg array
  cur_track = 0; //set current track position
  my_lock = 1; //create a lock for the the thread
  
  //set condition variables
  queue_full = num_threads+1;
  queue_open = num_threads+2;
  
  thread_libinit( (thread_startfunc_t) control, NULL);
  return 0;
}
