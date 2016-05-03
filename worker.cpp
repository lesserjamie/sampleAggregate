/** Write description here.

(c) 2016 Jamie Lesser, Emily Hoyt
*/

#include "worker.h"
#include "findPi.h"

int main(int argc, char** argv) {
  int MasterPort = 0;
  std::string MasterAddr = "";

  if (argc < 5) {
    printf("Failed to provide enough arguments.\nSee README.md\n");
    return 0;
  }

  // PARSE ARGS
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if ((i+1) < argc) {
      if (arg == MASTER_PORT_TAG) {
	MasterPort = atoi(argv[i+1]);
      }
      if (arg == MASTER_ADDR_TAG) {
	MasterAddr = argv[i+1];
      }
    }
  }

  if ((MasterPort < 0)
      || (MasterAddr == "")) {
    std::cout << "Invalid argument. Please see README.md." << std::endl;
    return 0;
  }

  db_out << "Double check:" << std::endl;
  db_out << "Master Port: " <<  MasterPort << std::endl;
  db_out << "Master Addr: " <<  MasterAddr.c_str() << std::endl;

  Worker<FindPi> w;
  if (w.init(MasterPort, MasterAddr) == 0) {
    w.run();
    w.cleanUp();
  }

  return 0;
}
