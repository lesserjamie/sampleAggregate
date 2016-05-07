/** Add description

    (c) 2016 Jamie Lesser, Emily Hoyt
*/
#ifndef MASTER_H
#define MASTER_H

#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <pthread.h>

static bool debug = false;
#define db_out if(debug) std::cout

static pthread_mutex_t running_mutex;
static pthread_mutex_t seenWorkers_mutex;
static pthread_mutex_t readyWorkers_mutex;
static pthread_mutex_t resultStrings_mutex;
static pthread_mutex_t threadVector_mutex;
static pthread_mutex_t numReceived_mutex;
static pthread_mutex_t numSendsInFlight_mutex;

static const std::string PORT_TAG = "-p";

template <class T>
class Master {
 public:
  int socketfd = -1;

  std::queue<int> seenWorkers;
  std::queue<int> readyWorkers;

  T aggregator;

  int numSendsInFlight = 0;

  std::vector<std::string> resultStrings;
  int numSamples = 0;
  int numReceived = 0;

  std::vector<pthread_t> threadVector;

  bool running = false;

  int initialize_socket(int port) {
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


  int initialize_threadpool() {
    // Initialize locks

    running_mutex = PTHREAD_MUTEX_INITIALIZER;
    seenWorkers_mutex = PTHREAD_MUTEX_INITIALIZER;
    readyWorkers_mutex = PTHREAD_MUTEX_INITIALIZER;
    resultStrings_mutex = PTHREAD_MUTEX_INITIALIZER;
    threadVector_mutex = PTHREAD_MUTEX_INITIALIZER;
    numReceived_mutex = PTHREAD_MUTEX_INITIALIZER;
    numSendsInFlight_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_t thread0;
    if(pthread_create(&thread0, NULL, Master<T>::accept_incoming_workers_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread0);

    pthread_t thread1;
    if(pthread_create(&thread1, NULL, Master<T>::process_seen_workers_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    pthread_t thread2;
    if(pthread_create(&thread2, NULL, Master<T>::process_ready_workers_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread2);
 
    return 0;
  }

  int clean_threadpool() {
    for(size_t i = 0; i < threadVector.size(); i++){
      if(pthread_cancel(threadVector[i]) < 0) {
	db_out << "Error cancelling thread" << std::endl;
	return -1;
      }
    }

  
    pthread_mutex_destroy(&running_mutex);

    return 0;
  }


  int sendMessage(int socket, std::string message) {
    pthread_mutex_lock(&numSendsInFlight_mutex);
    numSendsInFlight++;
    pthread_mutex_unlock(&numSendsInFlight_mutex);
  
    if(send(socket, message.c_str(), sizeof(char)*message.length(),0) > 0){
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

  int receiveMessage(int worker_socket) {
    char buffer[BUFSIZ];
    std::stringstream stream;
    std::string message;
    pthread_mutex_lock(&running_mutex);
    bool keep_looping = running;
    pthread_mutex_unlock(&running_mutex);

    while (recv(worker_socket,&buffer,BUFSIZ,0) > 0 && keep_looping) {
      stream << buffer;

    
      message = stream.str();
      if(message.find("\r\n\r\nEND\r\n\r\n") != std::string::npos) break;

      pthread_mutex_lock(&running_mutex);
      keep_looping = running;
      pthread_mutex_unlock(&running_mutex);

      std::memset(buffer, 0, BUFSIZ);
    }
  
    db_out << "Received message:\n" << message << std::endl;

    if (message.length() == 0) {
      db_out << "Failed to receive message." << std::endl;
    } else if (message.find_first_of("WORKER READY") == 0) {
   
      pthread_mutex_lock(&readyWorkers_mutex);
      readyWorkers.push(worker_socket);
      pthread_mutex_unlock(&readyWorkers_mutex);

      std::string reply = "WORKER ADDED\r\n\r\nEND\r\n\r\n";
      sendMessage(worker_socket, reply);
    
      pthread_mutex_lock(&seenWorkers_mutex);
      seenWorkers.push(worker_socket);
      pthread_mutex_unlock(&seenWorkers_mutex);

    } else if (message.find_first_of("TASK RESULT:\n") == 0) {

      std::string data = message.substr(14, message.length() - 25);

      db_out << "Data:" << data << ":" << std::endl;

      pthread_mutex_lock(&resultStrings_mutex);
      resultStrings.push_back(data);
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

  void* process_seen_workers(void) {
    pthread_mutex_lock(&running_mutex);
    bool keep_looping = running;
    pthread_mutex_unlock(&running_mutex);
  
    while(keep_looping) {
      int next = 0;
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

  void* process_ready_workers(void) {
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
	std::string message = "TASK PARAMS\r\n\r\nEND\r\n\r\n";
	sendMessage(next, message);
      }
    
      pthread_mutex_lock(&running_mutex);
      keep_looping = running;
      pthread_mutex_unlock(&running_mutex);
    
    }

    pthread_exit(NULL);
  }

  void* accept_incoming_workers(void) {
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

  static void* process_seen_workers_redirect(void* arg) {
    return ((Master<T>*)arg)->process_seen_workers();
  }

  static void* process_ready_workers_redirect(void* arg) {
    return ((Master<T>*)arg)->process_ready_workers();
  }

  static void* accept_incoming_workers_redirect(void* arg) {
    return ((Master<T>*)arg)->accept_incoming_workers();
  }

  Master(int numSamples) {
    this->numSamples = numSamples;
  }

  ~Master() {}
  
  int init(int port) {
    int socket = initialize_socket(port);
    if(socket >= 0) {
      socketfd = socket;
      return 0;
    }

    db_out << "Failed to initialize socket." << std::endl;

    return -1;
  }
 
  int run() {
    running = true;
    bool keep_looping = running;

    initialize_threadpool();

    while(keep_looping) {
      pthread_mutex_lock(&running_mutex);
      keep_looping = running;
      pthread_mutex_unlock(&running_mutex);
    }

    db_out << "Received samples:" << std::endl;
    for(size_t i = 0; i < resultStrings.size(); i++) {
      db_out << resultStrings[i] << std::endl;
    }


    std::string result = aggregator.aggregate(resultStrings);

    printf("Pi is about: %s\n", result.c_str());

    return 0;
  }

  int cleanUp() {
    // Need to let threads finish executing whatever they're currently doing
    bool needToWait = true;
    while(needToWait) {  
      pthread_mutex_lock(&numSendsInFlight_mutex);
      needToWait = (numSendsInFlight > 0);
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

};

#endif
