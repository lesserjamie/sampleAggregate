/** Add description

(c) 2016 Jamie Lesser, Emily Hoyt
*/
#ifndef WORKER_H
#define WORKER_H

#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <sstream>
#include <cstring>

static bool debug = true;
#define db_out if(debug) std::cout

static const std::string MASTER_PORT_TAG = "-mp";
static const std::string MASTER_ADDR_TAG = "-ma";

class Worker {
 private:
  int masterSocketfd = -1;

  int initialize_socket(int port, std::string name);
  int sendMessage(int socket, std::string message);
  int receiveMessage(int socket);

 public:
  Worker();
  ~Worker();

  int init(int port, std::string addr);
  int run();
  int cleanUp();
};

#endif
