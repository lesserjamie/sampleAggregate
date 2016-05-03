/**
Write description here.

(c) 2016 Jamie Lesser, Emily Hoyt
*/

#include "master.h"

typedef void* (Master::*MasterPtr)(void);
typedef void* (*PthreadPtr)(void*);

Master::Master(int numSamples){
  this->numSamples = numSamples;
}

Master::~Master(){}

int Master::init(int port) {
  int socket = initialize_socket(port);
  if(socket >= 0) {
    socketfd = socket;
    return 0;
  }

  db_out << "Failed to initialize socket." << std::endl;
  return -1;
}

int Master::run() {
  running = true;
  bool keep_looping = running;

  initialize_threadpool();

  while(keep_looping) {
    pthread_mutex_lock(&running_mutex);
    keep_looping = running;
    pthread_mutex_unlock(&running_mutex);
  }

  return 0;
}

void* Master::accept_incoming_workers(void) {
  pthread_mutex_lock(&running_mutex);
  bool keep_looping = running;
  pthread_mutex_unlock(&running_mutex);
  
  int worker_socket = 0;
  socklen_t sockAddrWorkerLength;
  struct sockaddr_in sockAddrWorker;
  
  sockAddrWorkerLength = sizeof(sockAddrWorker);
  
  while (keep_looping) {
    
    worker_socket = accept(socketfd,(struct sockaddr*)&sockAddrWorker, &sockAddrWorkerLength);
    
    db_out << "Successfully accepted " << std::endl;
    
    receiveMessage(worker_socket);
    //printf("IM IN THE FIRST THREAD, running = %d\n", running);
    
    
    pthread_mutex_lock(&running_mutex);
    keep_looping = running;
    pthread_mutex_unlock(&running_mutex);
  }

  pthread_exit(NULL);
}

void* Master::process_seen_workers(void) {
  pthread_mutex_lock(&running_mutex);
  bool keep_looping = running;
  pthread_mutex_unlock(&running_mutex);
  
  while(keep_looping) {
    int next = 0;

    //printf("IM IN THE SECOND THREAD\n");
      
    bool hasNext = false;

    pthread_mutex_lock(&seenWorkers_mutex);
    if (seenWorkers.size() > 0) {
      hasNext = true;
      next = seenWorkers.front();
      seenWorkers.pop();
    }
    pthread_mutex_unlock(&seenWorkers_mutex);
    
    if (hasNext) {
      receiveMessage(next);
    }
    
    pthread_mutex_lock(&running_mutex);
    keep_looping = running;
    pthread_mutex_unlock(&running_mutex);
    
  }

  pthread_exit(NULL);
}

void* Master::process_ready_workers(void) {
 pthread_mutex_lock(&running_mutex);
  bool keep_looping = running;
  pthread_mutex_unlock(&running_mutex);
  
  while(keep_looping) {
    int next = 0;

    //printf("IM IN THE SECOND THREAD\n");
    
    bool hasNext = false;
    
    pthread_mutex_lock(&readyWorkers_mutex);
    if (readyWorkers.size() > 0) {
      hasNext = true;
      next = readyWorkers.front();
      readyWorkers.pop();
    }
    pthread_mutex_unlock(&readyWorkers_mutex);
    
    if (hasNext) {
      std::string message = "TASK PARAMS/r/n/r/nEND/r/n/r/n";
      sendMessage(next, message);
    }
    
    pthread_mutex_lock(&running_mutex);
    keep_looping = running;
    pthread_mutex_unlock(&running_mutex);
    
  }

  pthread_exit(NULL);
}


int Master::sendMessage(int socket, std::string message) {
  pthread_mutex_lock(&numSendsInFlight_mutex);
  numSendsInFlight++;
  pthread_mutex_unlock(&numSendsInFlight_mutex);
  
  if(send(socket, message.c_str(), sizeof(message.c_str()),0) > 0){
    db_out << "Successfully sent:\n" << message << std::endl;
    pthread_mutex_lock(&numSendsInFlight_mutex);
    numSendsInFlight--;
    pthread_mutex_unlock(&numSendsInFlight_mutex);
    
    return 0;
  } else {
    db_out << "Failed to send." << std::endl;
    pthread_mutex_lock(&numSendsInFlight_mutex);
    numSendsInFlight--;
    pthread_mutex_unlock(&numSendsInFlight_mutex);
    
    return -1;
  }

}


// worker_socket not in seenWorkers this gets called
int Master::receiveMessage(int worker_socket) {
  char buffer[BUFSIZ];
  std::stringstream stream;
  
  pthread_mutex_lock(&running_mutex);
  bool keep_looping = running;
  pthread_mutex_unlock(&running_mutex);

  while (recv(worker_socket,buffer,sizeof(buffer),0) > 0 && keep_looping) {
    printf("IM RECEIVING, running = %d\n", running);

    stream << buffer;
    std::memset(buffer, 0, BUFSIZ);
    if(stream.str().find("\r\n\r\n") >= 0) break;
    pthread_mutex_lock(&running_mutex);
    keep_looping = running;
    pthread_mutex_unlock(&running_mutex);
  }
  
  std::string message = stream.str();
  
  if (message.length() == 0) {
    //db_out << "Failed to receive message." << std::endl;
  } else if (message.find_first_of("WORKER READY") == 0) {
   
    pthread_mutex_lock(&readyWorkers_mutex);
    readyWorkers.push(worker_socket);
    pthread_mutex_unlock(&readyWorkers_mutex);

    std::string reply = "WORKER ADDED\r\n\r\nEND\r\n\r\n";
    sendMessage(worker_socket, reply);
    
    pthread_mutex_lock(&seenWorkers_mutex);
    seenWorkers.push(worker_socket);
    pthread_mutex_unlock(&seenWorkers_mutex);

  } else if (message.find_first_of("TASK RESULT") == 0) {

    pthread_mutex_lock(&resultStrings_mutex);
    resultStrings.push_back(message.substr(message.find_first_of('\n') + 1));
    pthread_mutex_unlock(&resultStrings_mutex);

    pthread_mutex_lock(&numReceived_mutex);
    numReceived++;
    int numRCopy = numReceived;
    printf("%d out of %d\n", numReceived, numSamples);
    pthread_mutex_unlock(&numReceived_mutex);    

    if (numSamples <= numRCopy) {
      std::string closeMessage = "DONE\r\n\r\nEND\r\n\r\n";
      sendMessage(worker_socket, closeMessage);

      pthread_mutex_lock(&running_mutex);
      running = false;
      pthread_mutex_unlock(&running_mutex);
      
      db_out << "Set running to false\n" << std::endl;

    } else {
      pthread_mutex_lock(&seenWorkers_mutex);
      seenWorkers.push(worker_socket);
      pthread_mutex_unlock(&seenWorkers_mutex);
    }
  } else if (message.find_first_of("REMOVE WORKER") == 0) {
    pthread_mutex_lock(&seenWorkers_mutex);
    close(worker_socket);
    pthread_mutex_lock(&seenWorkers_mutex);
  }

 return 0;

}

int Master::cleanUp() {

  // Need to let threads finish executing whatever they're currently doing
  bool needToWait = true;
  while(needToWait) {  
    pthread_mutex_lock(&numSendsInFlight_mutex);
    needToWait = (numSendsInFlight > 0);
    db_out << numSendsInFlight << std::endl; 
    pthread_mutex_unlock(&numSendsInFlight_mutex);
  }

 clean_threadpool();
 
 std::string closeMessage = "DONE\r\n\r\nEND\r\n\r\n";
 
 while (seenWorkers.size() > 0) {
   int next = seenWorkers.front();
   seenWorkers.pop();
   sendMessage(next, closeMessage);
   close(next);
 }

  if (close(socketfd) == 0) {
    db_out << "Successfully closed socket." << std::endl;
    return 0;
  } 

  db_out << "Failed to close socket." << std::endl;
  return -1;
}

int Master::initialize_socket(int port) {
  int sock; 
  struct sockaddr_in sockAddrServer; 
  
  //Initialize socket
  sock = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
  
  //Initialize components of sockAddr struct
  sockAddrServer.sin_port = htons(port);
  sockAddrServer.sin_family = AF_INET;
  sockAddrServer.sin_addr.s_addr = htonl(INADDR_ANY);
  
  // Bind the socket
  if(bind(sock, (struct sockaddr*)&sockAddrServer, sizeof(sockAddrServer)) < 0) {
    db_out << "Failed to bind." << std::endl;
    return -1;
  }

  db_out << "Successfully called bind." << std::endl;
  
  // Tell the socket to listen for requests
  if(listen(sock,10) < 0) { 
    db_out << "Failed to listen." << std::endl;
    return -1;
  }

  db_out << "Successfully called listen." << std::endl;

  return sock; 
}

int Master::initialize_threadpool() {
  
  // Initialize locks

  running_mutex = PTHREAD_MUTEX_INITIALIZER;
  seenWorkers_mutex = PTHREAD_MUTEX_INITIALIZER;
  readyWorkers_mutex = PTHREAD_MUTEX_INITIALIZER;
  resultStrings_mutex = PTHREAD_MUTEX_INITIALIZER;
  threadVector_mutex = PTHREAD_MUTEX_INITIALIZER;
  numReceived_mutex = PTHREAD_MUTEX_INITIALIZER;
  numSendsInFlight_mutex = PTHREAD_MUTEX_INITIALIZER;
    
  pthread_t thread0;
  if(pthread_create(&thread0, NULL, Master::accept_incoming_workers_redirect, this) < 0) {
    db_out << "Failed to create thread" << std::endl;
    return -1;
  }

  threadVector.push_back(thread0);

  pthread_t thread1;
  if(pthread_create(&thread1, NULL, Master::process_seen_workers_redirect, this) < 0) {
    db_out << "Failed to create thread" << std::endl;
    return -1;
  }

  pthread_t thread2;
  if(pthread_create(&thread2, NULL, Master::process_ready_workers_redirect, this) < 0) {
    db_out << "Failed to create thread" << std::endl;
    return -1;
  }

  threadVector.push_back(thread2);
 
  return 0;
} 

int Master::clean_threadpool() {
  for(size_t i = 0; i < threadVector.size(); i++){
    if(pthread_cancel(threadVector[i]) < 0) {
      db_out << "Error cancelling thread" << std::endl;
      return -1;
    }
  }

  
  pthread_mutex_destroy(&running_mutex);

  return 0;
}

int main(int argc, char** argv) {
  int MasterPort = 0;

  if (argc < 3) {
    printf("Failed to provide enough arguments.\nSee README.md\n");
    return 0;
  }

  // PARSE ARGS
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if ((i+1) < argc) {
      if (arg == PORT_TAG) {
	MasterPort = atoi(argv[i+1]);
      }
    }
  }

  if (MasterPort < 0) {
    std::cout << "Invalid argument. Please see README.md." << std::endl;
    return 0;
  }

  db_out << "Double check:" << std::endl;
  db_out << "Master Port: " <<  MasterPort << std::endl;

  Master m(3);
  if (m.init(MasterPort) == 0) {
    m.run();
    m.cleanUp();
  }

  return 0;
}
