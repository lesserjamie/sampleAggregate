/** Add description

(c) 2016 Jamie Lesser, Emily Hoyt
*/
#ifndef MASTER_H
#define MASTER_H

#include <string>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

static bool debug = true;
#define db_out if(debug) std::cout

static const std::string PORT_TAG = "-p";

class Master {
 private:
  int socketfd = -1;

  int initialize_socket(int port);

 public:
  Master();
  ~Master();
  
  int init(int port);
  int run();
  int cleanUp();

};

#endif
