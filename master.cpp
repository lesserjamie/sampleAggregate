/**
Write description here.

(c) 2016 Jamie Lesser, Emily Hoyt
*/

#include "master.h"
#include "findPi.h"

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

  Master<FindPi> m(10000);
  if (m.init(MasterPort) == 0) {
    m.run();
    m.cleanUp();
  }

  return 0;
}
