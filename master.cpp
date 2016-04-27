/**
Write description here.

(c) 2016 Jamie Lesser, Emily Hoyt
*/

#include "master.h"

Master::Master(){}

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
  int worker_socket = 0;
  socklen_t sockAddrWorkerLength;
  struct sockaddr_in sockAddrWorker;

  sockAddrWorkerLength = sizeof(sockAddrWorker);

  int connection_count = 0;

  while(connection_count < 3) {
    worker_socket = accept(socketfd,(struct sockaddr*)&sockAddrWorker, &sockAddrWorkerLength);
    db_out << "Successfully accepted " << std::endl;
    
    
    char buffer[BUFSIZ];
    std::stringstream stream;

    while (recv(worker_socket,buffer,sizeof(buffer),0) > 0) {
      stream << buffer;
      std::memset(buffer, 0, BUFSIZ);
      if(stream.str().find("\r\n\r\n") >= 0) break;
    }

    std::string message = stream.str();
    if (message.length() == 0) {
      db_out << "Failed to receive message." << std::endl;
    } else {
      db_out << "Received:\n" << message.c_str() << std::endl;

      std::string response = "Hi back!\r\n\r\n";
      if (send(worker_socket, response.c_str(), sizeof(response.c_str()), 0) > 0) {
	db_out << "Successfully send response:\n" << response << std::endl;
      } else {
	db_out << "Failed to send response." << std::endl;
      }

    }

    connection_count++;
  }

  return 0;
}

int Master::cleanUp() {
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

  Master m;
  if (m.init(MasterPort) == 0) {
    m.run();
    m.cleanUp();
  }

  return 0;
}
