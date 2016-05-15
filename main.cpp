/**
Write description here.

(c) 2016 Jamie Lesser, Emily Hoyt
*/

#include "SampleAggregate.h"
#include "findPi.h"

static const std::string MASTER_TAG = "-m";
static const std::string PORT_TAG = "-p";
static const std::string ADDR_TAG = "-a";
static const std::string NUM_TAG = "-n";

int main(int argc, char** argv) {
  bool master = false;
  int port = 0;
  int n = 0;
  std::string addr = "";

  if (argc < 5) {
    printf("Failed to provide enough arguments.\nSee README.md\n");
    return 0;
  }

  // PARSE ARGS
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == MASTER_TAG) {
      master = true;
    }

    if ((i+1) < argc) {
      if (arg == PORT_TAG) {
	port = atoi(argv[i+1]);
      } else if (arg == NUM_TAG) {
	n = atoi(argv[i+1]);
      } else if (arg == ADDR_TAG) {
	addr = argv[i+1];
      }
    }
  }

  if (port < 0) {
    std::cout << "Invalid argument. Please see README.md." << std::endl;
    return 0;
  } else if (port < 8000) {
    std::cout << "Are you sure you want to use a port number less than 8000?" << std::endl;
  }

  if (master) {
    db_out << "Master Node" << std::endl;
    db_out << "Number of samples: " <<  n    << std::endl;
    db_out << "Port:              " <<  port << std::endl;
  } else {
    db_out << "Worker Node" << std::endl;
    db_out << "Master Address: " <<  addr << std::endl;
    db_out << "Master Port:    " <<  port << std::endl;
  }

  SampleAggregate<FindPi> sa;
  if (sa.init(port, addr, n, master) == 0) {
    clock_t begin;
    if (master) {
      begin = clock();
    }
    std::string result = sa.run();
    if (master) {
      std::cout << "Pi is about " << result << "." << std::endl;
      clock_t end = clock();
      double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
      std::cout << "Took " << elapsed_secs << "to compute."
    }
    sa.cleanUp();
  } 

  return 0;
}
