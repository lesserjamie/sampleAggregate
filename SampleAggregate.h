/** Add description

    (c) 2016 Jamie Lesser, Emily Hoyt
*/

#ifndef SAMPLE_AGGREGATE_H
#define SAMPLE_AGGREGATE_H

#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <strings.h>
#include <unistd.h>
#include <vector>
#include <queue>

static bool debug = true;
#define db_out if(debug) std::cout

const std::string MSG_END_STR    = "\r\n\r\nEND\r\n\r\n";
const std::string JOB_DONE_STR   = "JOB DONE";
const std::string SMP_PACKET_STR = "SAMPLE RESULT:\n";

static pthread_mutex_t is_running_mutex;
static pthread_mutex_t workers_mutex;
static pthread_mutex_t sample_packets_mutex;
static pthread_mutex_t thread_vector_mutex;
static pthread_mutex_t num_samples_mutex;
static pthread_mutex_t num_sending_mutex;

template <class T>
class Master {
 public:
  Master() {}
  ~Master() {
    delete aggregator_;
    aggregator_ = NULL;
  }
  
  int init(int port, size_t num_samples) {
    int socketfd = initializeSocket(port);
    num_samples_ = num_samples;

    if (socketfd < 0) {
      std::cerr << "Failed to initialize socket." << std::endl;
      return -1;
    } 

    socketfd_ = socketfd;
    return 0;
  }
 
  std::string run() {
    bool is_running = true;
    is_running_ = true;
    initializeThreadpool();

    while (is_running) {
      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }

    return aggregator_->aggregate(sample_packets_);
  }

  int cleanUp() {
    bool wait_for_send = true;
    int next;

    while (wait_for_send) {  
      pthread_mutex_lock(&num_sending_mutex);
      wait_for_send = (num_sending_ > 0);
      pthread_mutex_unlock(&num_sending_mutex);
    }

    cleanThreadpool();
  
    while (workers_.size() > 0) {
      next = workers_.front();
      workers_.pop();
      sendMessage(next, JOB_DONE_STR + MSG_END_STR);
      close(next);
    }

    if (close(socketfd_) < 0) {
      std::cerr << "Failed to close socket." << std::endl;
      return -1;
    } 
    
    return 0;
  }

 private:
  std::queue<int> workers_;                 // Worker socket filed descriptors. 
  std::vector<std::string> sample_packets_; // Data strings from sample packets.
  std::vector<pthread_t> thread_vector_;    // Thread pool.
  T *aggregator_    = NULL;  // Object used to aggregate sample packets once 
                             //   _num_samples sample packets are received.
  bool is_running_  = false; // Signals whether or not currently collecting 
                             //   sample packets from worker nodes.
  int socketfd_     = -1;    // File descriptor of socket being used to send 
                             //   and receive messages to/from worker nodes.
  int num_sending_  = 0;     // Number of threads sending a message over the 
                             //   network. Prevents cutting messages short or 
                             //   sending trash messages.
  size_t num_samples_  = 0;  // Goal number of sample packets to be received.

  int initializeSocket(int port) {
    int socketfd; 
    struct sockaddr_in sock_addr; 
  
    //Initialize socket
    socketfd = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
  
    //Initialize components of sockAddr struct
    sock_addr.sin_port = htons(port);
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
    // Bind the socket
    if (bind(socketfd, (struct sockaddr*)&sock_addr, 
	    sizeof(sock_addr)) < 0) {
      std::cerr << "Failed to bind." << std::endl;
      return -1;
    }
  
    // Tell the socket to listen for requests
    if (listen(socketfd,10) < 0) { 
      std::cerr << "Failed to listen." << std::endl;
      return -1;
    }

    return socketfd; 
  }


  int initializeThreadpool() {
    pthread_t thread;
    int i;

    // Initialize locks
    is_running_mutex = PTHREAD_MUTEX_INITIALIZER;
    workers_mutex = PTHREAD_MUTEX_INITIALIZER;
    sample_packets_mutex = PTHREAD_MUTEX_INITIALIZER;
    thread_vector_mutex = PTHREAD_MUTEX_INITIALIZER;
    num_samples_mutex = PTHREAD_MUTEX_INITIALIZER;
    num_sending_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    for (i = 0; i < 1; ++i) {
      if (pthread_create(&thread, NULL, 
			 Master<T>::acceptIncomingWorkersRedirect, 
			 this) < 0) {
	std::cerr << "Failed to create thread." << std::endl;
      } else {
	thread_vector_.push_back(thread);
      }
    }

    for (i = 0; i < 2; ++i) {
      if (pthread_create(&thread, NULL, 
			 Master<T>::processWorkersRedirect, 
			 this) < 0) {
	std::cerr << "Failed to create thread." << std::endl;
      } else {
	thread_vector_.push_back(thread);
      }
    }

    if (thread_vector_.size() == 0) {
      std::cerr << "Failed to initialize thread pool." <<std::endl;
      return -1;
    }
    return 0;
  }

  int cleanThreadpool() {
    for (size_t i = 0; i < thread_vector_.size(); ++i){
      if (pthread_cancel(thread_vector_[i]) < 0) {
	std::cerr << "Failed to cancel thread." << std::endl;
      }
    }

    pthread_mutex_destroy(&is_running_mutex);
    pthread_mutex_destroy(&workers_mutex);
    pthread_mutex_destroy(&sample_packets_mutex);
    pthread_mutex_destroy(&thread_vector_mutex);
    pthread_mutex_destroy(&num_samples_mutex);
    pthread_mutex_destroy(&num_sending_mutex);

    return 0;
  }


  int sendMessage(int socketfd, std::string message) {
    pthread_mutex_lock(&num_sending_mutex);
    num_sending_++;
    pthread_mutex_unlock(&num_sending_mutex);

    if (send(socketfd, message.c_str(), sizeof(char)*message.length(), 0) <= 0) {
      std::cerr << "Failed to send message." << std::endl;
      
      pthread_mutex_lock(&num_sending_mutex);
      num_sending_--;
      pthread_mutex_unlock(&num_sending_mutex);
    
      return -1;
    }
    
    pthread_mutex_lock(&num_sending_mutex);
    num_sending_--;
    pthread_mutex_unlock(&num_sending_mutex);
    
    return 0;
  }

  int receiveMessage(int socketfd) {
    size_t bite = sizeof(char);
    char buffer[bite];
    bool is_running;
    std::stringstream stream;
    std::string message;

    pthread_mutex_lock(&is_running_mutex);
    is_running = is_running_;
    pthread_mutex_unlock(&is_running_mutex);

    while ((recv(socketfd,&buffer,bite,0) > 0) && is_running) {
      buffer[bite] = '\0';
      stream << buffer;
      std::memset(buffer, 0, bite);

      message = stream.str();
      if(message.find(MSG_END_STR) != std::string::npos && 
	 ((message.length() - MSG_END_STR.length()) == 
	  message.find(MSG_END_STR))) break;

      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }

    if (message.length() == 0) {
      std::cerr << "Failed to receive message." << std::endl;
      return -1;
    }  else if ((message.find_first_of(SMP_PACKET_STR) != std::string::npos) && 
		(message.find_first_of(SMP_PACKET_STR) == 0)) {
      size_t num_samples;
      size_t num_received;
      std::string data = message.substr(SMP_PACKET_STR.length(), 
					message.length() - 
					SMP_PACKET_STR.length() - 
					MSG_END_STR.length());
            
      pthread_mutex_lock(&sample_packets_mutex);
      sample_packets_.push_back(data);
      num_received = sample_packets_.size();
      pthread_mutex_unlock(&sample_packets_mutex);
      
      pthread_mutex_lock(&num_samples_mutex);
      num_samples = num_samples_;
      pthread_mutex_unlock(&num_samples_mutex);
      
      db_out << (100*(num_received/(float)num_samples));
      db_out << " percent of samples received." << std::endl;
	
	if (num_samples <= num_received) {
	  sendMessage(socketfd, JOB_DONE_STR + MSG_END_STR);
	  
	  pthread_mutex_lock(&is_running_mutex);
	  is_running_ = false;
	  pthread_mutex_unlock(&is_running_mutex);
	} else {
	  pthread_mutex_lock(&workers_mutex);
	  workers_.push(socketfd);
	  pthread_mutex_unlock(&workers_mutex);
	}
    }

    return 0;
  }

  void* processWorkers(void) {
    bool hasNext = false;
    int next = 0;
    bool is_running;

    pthread_mutex_lock(&is_running_mutex);
    is_running = is_running_;
    pthread_mutex_unlock(&is_running_mutex);
  
    while(is_running) {
      hasNext = false;

      pthread_mutex_lock(&workers_mutex);
      if (workers_.size() > 0) {
	hasNext = true;
	next = workers_.front();
	workers_.pop();
      }
      pthread_mutex_unlock(&workers_mutex);
    
      if (hasNext) {
	receiveMessage(next);
      }
    
      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }

    pthread_exit(NULL);
  }

  void* acceptIncomingWorkers(void) {
    struct sockaddr_in sock_addr;
    socklen_t sock_addr_length = sizeof(sock_addr);
    int socketfd = 0;
    bool is_running;

    pthread_mutex_lock(&is_running_mutex);
    is_running = is_running_;
    pthread_mutex_unlock(&is_running_mutex);
  
    while (is_running) {

      socketfd = accept(socketfd_, (struct sockaddr*)&sock_addr, 
			&sock_addr_length);
    
      receiveMessage(socketfd);
    
      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }

    pthread_exit(NULL);
  }

  static void* processWorkersRedirect(void* arg) {
    return ((Master<T>*)arg)->processWorkers();
  }

  static void* acceptIncomingWorkersRedirect(void* arg) {
    return ((Master<T>*)arg)->acceptIncomingWorkers();
  } 
}; 

template <class T>
class Worker {
 public:
  Worker(){}
  ~Worker(){
    delete sampler_;
    sampler_ = NULL;
  }

  int init(int port, std::string addr) {
    int socketfd = initializeSocket(port, addr);
    if (socketfd < 0) {
      std::cerr << "Failed to initialize socket" << std::endl;
      return -1;
    }

    master_socketfd_ = socketfd;
    sampler_ = new T();
    return 0;
  }

  std::string run() {
    is_running_ = true;
    bool is_running = is_running_;

    initializeThreadpool();

    while(is_running) {
      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }
    
    return "";
  }

  int cleanUp() {
    cleanThreadpool();
  
    //Assume valid socket
    if (close(master_socketfd_) < 0) {
      std::cerr << "Failed to close socket." << std::endl;
      return -1;
    }

    return 0;
  }

 private:
  std::vector<pthread_t>  thread_vector_;
  T* sampler_ = NULL;
  int master_socketfd_ = -1;
  bool is_running_ = false;
  
  int initializeSocket(int port, std::string name) {
    struct hostent *hp = NULL;
    struct sockaddr_in sock_addr; 
    int socketfd; 
    
    //Initialize socket
    socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    //Initialize components of sockAddr struct
    sock_addr.sin_port = htons(port);
    sock_addr.sin_family = AF_INET;
    
    hp = gethostbyname(name.c_str());
    bcopy(hp->h_addr, &(sock_addr.sin_addr.s_addr), hp->h_length);

    if (connect(socketfd, (struct sockaddr*)&sock_addr, 
		sizeof(sock_addr)) < 0) {
      std::cerr << "Failed to connect." << std::endl;
      return -1;
    }

    return socketfd; 
  }

  int sendMessage(int socket, std::string message) {
    if(send(socket, message.c_str(), sizeof(char)*message.length(),0) <= 0){
      std::cerr << "Failed to send message." << std::endl;
      return -1;
    }

    return 0;
  }
  
  int receiveMessage(int socketfd) {
    size_t bite = sizeof(char);
    char buffer[bite];
    bool is_running;
    std::stringstream stream;
    std::string message;

    pthread_mutex_lock(&is_running_mutex);
    is_running = is_running_;
    pthread_mutex_unlock(&is_running_mutex);

    while ((recv(socketfd,&buffer,bite,0) > 0) && is_running) {
      buffer[bite] = '\0';
      stream << buffer;
      std::memset(buffer, 0, bite);

      message = stream.str();
      if(message.find(MSG_END_STR) != std::string::npos && 
	 ((message.length() - MSG_END_STR.length()) == 
	  message.find(MSG_END_STR))) break;

      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }
    
    if (message.length() == 0) {
      std::cerr << "Failed to receive message." << std::endl;
      return -1;
    } else if ((message.find_first_of(JOB_DONE_STR) != std::string::npos) && 
	       (message.find_first_of(JOB_DONE_STR) == 0)) {

      pthread_mutex_lock(&is_running_mutex);
      is_running_ = false;
      pthread_mutex_unlock(&is_running_mutex);    
    } 
    
    return 0;
  }

  void* processMessages(void) {
    pthread_mutex_lock(&is_running_mutex);
    bool is_running = is_running_;
    pthread_mutex_unlock(&is_running_mutex);

    while(is_running) {
      receiveMessage(master_socketfd_);

      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }
    
    pthread_exit(NULL);
  }

  void* sendSamples(void) {
    pthread_mutex_lock(&is_running_mutex);
    bool is_running = is_running_;
    pthread_mutex_unlock(&is_running_mutex);

    while(is_running) {
      std::string data = sampler_->sample();
      std::string outgoing = SMP_PACKET_STR + data + MSG_END_STR;
      
      sendMessage(master_socketfd_, outgoing);

      pthread_mutex_lock(&is_running_mutex);
      is_running = is_running_;
      pthread_mutex_unlock(&is_running_mutex);
    }
    
    pthread_exit(NULL);
  }

  static void* processMessagesRedirect(void* arg) {
    return ((Worker<T>*)arg)->processMessages();
  }

  static void* sendSamplesRedirect(void* arg) {
    return ((Worker<T>*)arg)->sendSamples();
  }

  int initializeThreadpool() {
    pthread_t thread;
    int i;

    // Initialize locks
    is_running_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (i = 0; i < 1; ++i) {
      if (pthread_create(&thread, NULL, 
			 Worker<T>::processMessagesRedirect, 
			 this) < 0) {
	std::cerr << "Failed to create thread." << std::endl;
      } else {
	thread_vector_.push_back(thread);
      }
    }

    for (i = 0; i < 1; ++i) {
      if (pthread_create(&thread, NULL, 
			 Worker<T>::sendSamplesRedirect, 
			 this) < 0) {
	std::cerr << "Failed to create thread." << std::endl;
      } else {
	thread_vector_.push_back(thread);
      }
    }

    return 0;
  }

  int cleanThreadpool() {
    for (size_t i = 0; i < thread_vector_.size(); ++i){
      if (pthread_cancel(thread_vector_[i]) < 0) {
	std::cerr << "Failed to cancel thread." << std::endl;
      }
    }

    pthread_mutex_destroy(&is_running_mutex);

    return 0;
  }
};

template <class T>
class SampleAggregate {


 public:
  SampleAggregate(){}
  
  ~SampleAggregate(){
    delete master_;
    master_ = NULL;

    delete worker_;
    worker_ = NULL;
  }

  //General init method
  int init(int port, std::string addr, int num, bool is_master) {
    is_master_ = is_master;
    if (is_master) return init(port, num);
    return init(port, addr);
  }

  //Init method for master node
  int init(int port, int num) {
    is_master_ = true;
    master_ = new Master<T>();
    return master_->init(port, num);
  }

  //Init method for worker node
  int init(int port, std::string addr) {
    is_master_ = false;
    worker_ = new Worker<T>();
    return worker_->init(port, addr);
  }

  std::string run() {
    if (is_master_) return master_->run();
    return worker_->run();
  }

  int cleanUp() {
    if (is_master_) return master_->cleanUp();
    return worker_->cleanUp();
  }

 private: 
  Worker<T>* worker_ = NULL;
  Master<T>* master_ = NULL;
  bool is_master_    = false;
};

#endif // SAMPLE_AGGREGATE_H
