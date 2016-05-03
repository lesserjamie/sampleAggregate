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

static bool debug = true;
#define db_out if(debug) std::cout

static pthread_mutex_t running_mutex;
static pthread_mutex_t seenWorkers_mutex;
static pthread_mutex_t readyWorkers_mutex;
static pthread_mutex_t resultStrings_mutex;
static pthread_mutex_t threadVector_mutex;
static pthread_mutex_t numReceived_mutex;
static pthread_mutex_t numSendsInFlight_mutex;

static const std::string PORT_TAG = "-p";

class Master {
 public:
  int socketfd = -1;

  std::queue<int> seenWorkers;
  std::queue<int> readyWorkers;

  int numSendsInFlight = 0;

  std::vector<std::string> resultStrings;
  int numSamples = 0;
  int numReceived = 0;

  std::vector<pthread_t> threadVector;

  bool running = false;

  int initialize_socket(int port);
  int initialize_threadpool();
  int clean_threadpool();
  int sendMessage(int socket, std::string message);
  int receiveMessage(int worker_socket);

  void* process_seen_workers(void);
  void* process_ready_workers(void);
  void* accept_incoming_workers(void);

  static void* process_seen_workers_redirect(void* arg) {
    return ((Master*)arg)->process_seen_workers();
  }

  static void* process_ready_workers_redirect(void* arg) {
    return ((Master*)arg)->process_ready_workers();
  }

  static void* accept_incoming_workers_redirect(void* arg) {
    return ((Master*)arg)->accept_incoming_workers();
  }

  Master(int numSamples);
  ~Master();
  
  int init(int port);
  int run();
  int cleanUp();

};

#endif
