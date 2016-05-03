/** Write description here.

(c) 2016 Jamie Lesser, Emily Hoyt
*/

#include "worker.h"

Worker::Worker() {
}

Worker::~Worker() {
}

int Worker::init(int port, std::string addr) {
  int socket = initialize_socket(port, addr);
  if (socket >= 0) {
    masterSocketfd = socket;
    return 0;
  }

  db_out << "Failed to initialize socket" << std::endl;
  return -1;
}

int Worker::run() {
  running = true;

  std::string message = "WORKER READY\r\n\r\nEND\r\n\r\n";
  sendMessage(masterSocketfd, message);

  while(running) {
    receiveMessage(masterSocketfd);
  }
  
  /*message = "TASK RESULT\r\n\r\nEND\r\n\r\n";
  sendMessage(masterSocketfd, message);
  
  message = "REMOVE WORKER\r\n\r\nEND\r\n\r\n";
  sendMessage(masterSocketfd, message);
  */

  return 0;
}

int Worker::receiveMessage(int socket) {
   char buffer[BUFSIZ];
   std::stringstream stream;
   
   while (recv(socket,buffer,sizeof(buffer),0) > 0) {
     stream << buffer;
     std::memset(buffer, 0, BUFSIZ);

     if(stream.str().find("\r\n\r\nEND\r\n\r\n") >= 0) break;
   }

   std::string message = stream.str();
   if (message.length() == 0) {
     //db_out << "Failed to receive message." << std::endl;
   } else if (message.find_first_of("DONE") == 0) {
     db_out << "Received message D:\n" << message << std::endl;
     db_out << "Set running to false" << std::endl;
     running = false;
   } else if (message.find_first_of("WORKER ADDED") == 0) {
     db_out << "Received message WA :\n" << message << std::endl;
     // handle ack?
   } else if (message.find_first_of("TASK PARAMS") == 0) {
     db_out << "Received message TP :\n" << message << std::endl;
     std::string outgoing = "TASK RESULT\r\n\r\nEND\r\n\r\n";
     sendMessage(socket, outgoing);
     
     outgoing = "WORKER READY\r\n\r\nEND\r\n\r\n";
     sendMessage(socket, outgoing);
   } 

   return 0;
}

int Worker::initialize_socket(int port, std::string name) {
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

int Worker::sendMessage(int socket, std::string message) {
  if(send(socket, message.c_str(), sizeof(message.c_str()),0) > 0){
    db_out << "Successfully sent:\n" << message << std::endl;
    return 0;
  } else {
    db_out << "Failed to send." << std::endl;
    return -1;
  }
}

int Worker::cleanUp() {
  //Assume valid socket
  if (close(masterSocketfd) == 0) {
    db_out << "Successfully closed socket." << std::endl;
    return 0;

  }

  db_out << "Failed to close socket." << std::endl;
  return -1;
}

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

  Worker w;
  if (w.init(MasterPort, MasterAddr) == 0) {
    w.run();
    w.cleanUp();
  }

  return 0;
}
