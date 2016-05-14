/** Add description

    (c) 2016 Jamie Lesser, Emily Hoyt
*/
#ifndef SAMPLEAGGREGATE_H
#define SAMPLEAGGREGATE_H

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <sstream>
#include <cstring>
#include <vector>
#include <queue>
#include <algorithm>
#include <pthread.h>

static bool debug = true;
#define db_out if(debug) std::cout

static pthread_mutex_t running_mutex;
static pthread_mutex_t seenWorkers_mutex;
static pthread_mutex_t resultStrings_mutex;
static pthread_mutex_t threadVector_mutex;
static pthread_mutex_t numReceived_mutex;
static pthread_mutex_t numSamples_mutex;
static pthread_mutex_t numSendsInFlight_mutex;

template <class T>
class Master {
 public:
  int socketfd = -1;

  std::queue<int> seenWorkers;

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
    resultStrings_mutex = PTHREAD_MUTEX_INITIALIZER;
    threadVector_mutex = PTHREAD_MUTEX_INITIALIZER;
    numReceived_mutex = PTHREAD_MUTEX_INITIALIZER;
    numSamples_mutex = PTHREAD_MUTEX_INITIALIZER;
    numSendsInFlight_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_t thread0;
    if(pthread_create(&thread0, NULL, Master<T>::accept_incoming_workers_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread0);

    pthread_t thread3;
    if(pthread_create(&thread3, NULL, Master<T>::process_seen_workers_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread3);

    pthread_t thread4;
    if(pthread_create(&thread4, NULL, Master<T>::process_seen_workers_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread4);

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
    pthread_mutex_destroy(&seenWorkers_mutex);
    pthread_mutex_destroy(&resultStrings_mutex);
    pthread_mutex_destroy(&threadVector_mutex);
    pthread_mutex_destroy(&numReceived_mutex);
    pthread_mutex_destroy(&numSamples_mutex);
    pthread_mutex_destroy(&numSendsInFlight_mutex);

    return 0;
  }


  int sendMessage(int socket, std::string message) {
    pthread_mutex_lock(&numSendsInFlight_mutex);
    numSendsInFlight++;
    pthread_mutex_unlock(&numSendsInFlight_mutex);
  
    if(send(socket, message.c_str(), sizeof(char)*message.length(),0) > 0){
      //db_out << "Successfully sent:\n" << message << std::endl;
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
    size_t bite = sizeof(char);

    char buffer[bite];
    std::stringstream stream;
    std::string message;
    pthread_mutex_lock(&running_mutex);
    bool keep_looping = running;
    pthread_mutex_unlock(&running_mutex);

    db_out << "In receive message for " << worker_socket << std::endl;

    while ((recv(worker_socket,&buffer,bite,0) > 0) && keep_looping) {
      buffer[bite] = '\0';
      stream << buffer;

      message = stream.str();
      if(message.find("\r\n\r\nEND\r\n\r\n") != std::string::npos) break;

      pthread_mutex_lock(&running_mutex);
      keep_looping = running;
      pthread_mutex_unlock(&running_mutex);

      std::memset(buffer, 0, bite);
    }
  
    
    // shave off junk at end
    message = message.substr(0, message.length() - message.find_first_of("\r\n\r\nEND\r\n\r\n") + std::string("\r\n\r\nEND\r\n\r\n").length());

    db_out << "Received message:\n" << message << std::endl;
    db_out << "npos is: " << std::string::npos << std::endl;
    db_out << "Result of find first worker ready: " << message.find_first_of("WORKER READY") << std::endl;
    db_out << "Result of find first task result: " << message.find_first_of("TASK RESULT") << std::endl;


    
    


    if (message.length() == 0) {
      db_out << "Failed to receive message." << std::endl;
    } else if ((message.find_first_of("WORKER READY") != std::string::npos) && (message.find_first_of("WORKER READY") == 0)) {

      db_out << message.find_first_of("WORKER READY") << std::endl;

      db_out << "Received WORKER READY from " << worker_socket << std::endl;

      pthread_mutex_lock(&seenWorkers_mutex);
      seenWorkers.push(worker_socket);
      pthread_mutex_unlock(&seenWorkers_mutex);

      //std::string reply = "WORKER ADDED\r\n\r\nEND\r\n\r\n";
      //sendMessage(worker_socket, reply);

    } else if ((message.find_first_of("TASK RESULT") != std::string::npos) && (message.find_first_of("TASK RESULT") == 0)) {

      db_out << message.find_first_of("TASK RESULT") << std::endl;

      db_out << "Received TASK RESULT from " << worker_socket << std::endl;

      int header_length = std::string("TASK RESULT: \n").length();
      int ender_length = std::string("\r\n\r\nEND\r\n\r\n").length();

      std::string data = message.substr(header_length, message.length() - header_length - ender_length);

      db_out << "Data:" << data << ":" << std::endl;

      pthread_mutex_lock(&resultStrings_mutex);
      resultStrings.push_back(data);
      pthread_mutex_unlock(&resultStrings_mutex);

      pthread_mutex_lock(&numReceived_mutex);
      numReceived++;
      int numRCopy = numReceived;
      //printf("%d out of %d\n", numReceived, numSamples);
      pthread_mutex_unlock(&numReceived_mutex);    


      pthread_mutex_lock(&numSamples_mutex);
      int numSamps = numSamples;
      pthread_mutex_unlock(&numSamples_mutex);
      
      //int update = 400;

      //if (numRCopy > 0 && ((update*(numRCopy/(float)numSamps))/(float)update - (int)(update*(numRCopy/(float)numSamps))/(float)update) == 0) {
	printf("%f percent of samples received.\n",(100*(numRCopy/(float)numSamps)));
	//}

      if (numSamps <= numRCopy) {
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
    } else if ((message.find_first_of("REMOVE WORKER") != std::string::npos) && (message.find_first_of("REMOVE WORKER") == 0)) {

      db_out << "Received REMOVE WORKER from " << worker_socket << std::endl;

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
      //db_out << seenWorkers.size() << " workers in the queue" << std::endl;
      if (seenWorkers.size() > 0) {
	hasNext = true;
	next = seenWorkers.front();
	db_out << "Looking at " << next << std::endl;
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
    
      db_out << "Successfully accepted " << worker_socket << std::endl;
    
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

  static void* accept_incoming_workers_redirect(void* arg) {
    return ((Master<T>*)arg)->accept_incoming_workers();
  }

  Master() {}

  ~Master() {}
  
  int init(int port, int numSamples) {
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

template <class T>
class Worker {
 private:
  T sampler;

  int masterSocketfd = -1;
  std::queue<std::string> requests;

  std::vector<pthread_t> threadVector;

  bool running = false;
  
  int initialize_socket(int port, std::string name) {
    int sock; 
    struct sockaddr_in sockAddrServer; 
    
    //Initialize socket
    sock = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
    
    //Initialize components of sockAddr struct
    sockAddrServer.sin_port = htons(port);
    sockAddrServer.sin_family = AF_INET;
    
    struct hostent *hp = gethostbyname(name.c_str());
    bcopy(hp->h_addr, &(sockAddrServer.sin_addr.s_addr), hp->h_length);

    if(connect(sock, (struct sockaddr*)&sockAddrServer, sizeof(sockAddrServer)) < 0) {
      db_out << "Failed to connect." << std::endl;
      return -1;
    }
    db_out << "Successfully called connect." << std::endl;

    return sock; 
  }

  int sendMessage(int socket, std::string message) {
    if(send(socket, message.c_str(), sizeof(char)*message.length(),0) > 0){
      db_out << "Successfully sent:\n" << message << std::endl;
      return 0;
    } else {
      db_out << "Failed to send message." << std::endl;
      return 1;
    }

  }
  
  int receiveMessage(int socket) {
    char buffer[BUFSIZ];
    std::stringstream stream;
    
    while (recv(socket,&buffer,BUFSIZ,0) > 0) {
      stream << buffer;
      std::memset(buffer, 0, BUFSIZ);
      
      if(stream.str().find("\r\n\r\nEND\r\n\r\n") != std::string::npos) break;
    }
    
    std::string message = stream.str();
    if (message.length() == 0) {
      //db_out << "Failed to receive message." << std::endl;
    } else if ((message.find_first_of("DONE") != std::string::npos) && (message.find_first_of("DONE") == 0)) {
      db_out << "Received message D" << std::endl;
      db_out << "Set running to false" << std::endl;
      pthread_mutex_lock(&running_mutex);
      running = false;
      pthread_mutex_unlock(&running_mutex);
    
    } else if ((message.find_first_of("WORKER ADDED") != std::string::npos) && (message.find_first_of("WORKER ADDED") == 0)) {
      db_out << "Received message WA" << std::endl;
      // handle ack?
    }
    
    return 0;
  }

  void* process_messages(void) {
    pthread_mutex_lock(&running_mutex);
    bool keep_looping = running;
    pthread_mutex_unlock(&running_mutex);

    while(keep_looping) {
      receiveMessage(masterSocketfd);

      pthread_mutex_lock(&running_mutex);
      keep_looping = running;
      pthread_mutex_unlock(&running_mutex);
    }
    
    pthread_exit(NULL);
  }

  void* send_samples(void) {
    pthread_mutex_lock(&running_mutex);
    bool keep_looping = running;
    pthread_mutex_unlock(&running_mutex);

    while(keep_looping) {
      std::string data = sampler.sample();
      std::string outgoing = "TASK RESULT: \n" + data + "\r\n\r\nEND\r\n\r\n";
      
      sendMessage(masterSocketfd, outgoing);

      pthread_mutex_lock(&running_mutex);
      keep_looping = running;
      db_out << "Running: " << running << std::endl;
      pthread_mutex_unlock(&running_mutex);
    }
    
    pthread_exit(NULL);
  }

  static void* process_messages_redirect(void* arg) {
    return ((Worker<T>*)arg)->process_messages();
  }

  static void* send_samples_redirect(void* arg) {
    return ((Worker<T>*)arg)->send_samples();
  }

  int initialize_threadpool() {
    // Initialize locks
    running_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t thread0;
    if(pthread_create(&thread0, NULL, Worker<T>::process_messages_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread0);

    pthread_t thread1;
    if(pthread_create(&thread1, NULL, Worker<T>::send_samples_redirect, this) < 0) {
      db_out << "Failed to create thread" << std::endl;
      return -1;
    }

    threadVector.push_back(thread1);

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

 public:
  Worker(){}
  ~Worker(){}

  int init(int port, std::string addr) {
    int socket = initialize_socket(port, addr);
    if (socket >= 0) {
      masterSocketfd = socket;
      return 0;
    }
    
    db_out << "Failed to initialize socket" << std::endl;

    //    T sampler;
    //this->sampler = sampler;

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
    
    /*message = "TASK RESULT\r\n\r\nEND\r\n\r\n";
      sendMessage(masterSocketfd, message);
      
      message = "REMOVE WORKER\r\n\r\nEND\r\n\r\n";
      sendMessage(masterSocketfd, message);
    */
    
    return 0;
  }

  int cleanUp() {
    clean_threadpool();
  
    //Assume valid socket
    if (close(masterSocketfd) == 0) {
      db_out << "Successfully closed socket." << std::endl;
      return 0;

    }

    db_out << "Failed to close socket." << std::endl;
    return -1;
  }

};

template <class T>
class SampleAggregate {
 private: 
  bool isMaster = false;
  Worker<T> worker;
  Master<T> master;

 public:
  SampleAggregate(){}
  ~SampleAggregate(){}

  //General init method
  int init(int port, std::string addr, int num, bool isMaster) {
    this->isMaster = isMaster;
    if (isMaster) return init(port, num);
    else return init(port, addr);
  }

  //Init method for master node
  int init(int port, int num) {
    isMaster = true;
    return master.init(port, num);
  }

  //Init method for worker node
  int init(int port, std::string addr) {
    isMaster = false;
    return worker.init(port, addr);
  }

  int run() {
    if (isMaster) return master.run();
    return worker.run();
  }

  int cleanUp() {
    if (isMaster) return master.cleanUp();
    return worker.cleanUp();
  }

};

#endif
