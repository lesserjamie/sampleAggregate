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
#include <vector>
#include <queue>
#include <algorithm>
#include <pthread.h>

static bool debug = false;
#define db_out if(debug) std::cout

static const std::string MASTER_PORT_TAG = "-mp";
static const std::string MASTER_ADDR_TAG = "-ma";

template <class T>
class Worker {
 private:
  std::vector<T> samplers;

  int masterSocketfd = -1;
  std::queue<std::string> requests;
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
      return 0;
    } else {
      return 1;
    }

  }
  
  int receiveMessage(int socket) {
    char buffer[BUFSIZ];
    std::stringstream stream;
    
    while (recv(socket,&buffer,BUFSIZ,0) > 0) {
      stream << buffer;
      std::memset(buffer, 0, BUFSIZ);
      
      if(stream.str().find("\r\n\r\nEND\r\n\r\n") >= 0) break;
    }
    
    std::string message = stream.str();
    if (message.length() == 0) {
      //db_out << "Failed to receive message." << std::endl;
    } else if (message.find_first_of("DONE") == 0) {
      db_out << "Received message D" << std::endl;
      db_out << "Set running to false" << std::endl;
      running = false;
    } else if (message.find_first_of("WORKER ADDED") == 0) {
      db_out << "Received message WA" << std::endl;
      // handle ack?
    } else if (message.find_first_of("TASK PARAMS") == 0) {
      db_out << "Received message TP" << std::endl;

      std::string data = samplers[0].sample();
      std::string outgoing = "TASK RESULT: \n" + data + "\r\n\r\nEND\r\n\r\n";
      
      sendMessage(socket, outgoing);
      
      outgoing = "WORKER READY\r\n\r\nEND\r\n\r\n";
      sendMessage(socket, outgoing);
    } 
    
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

    T sampler;
    samplers.push_back(sampler);

    return -1;
  }

  int run() {
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

  int cleanUp() {
  //Assume valid socket
  if (close(masterSocketfd) == 0) {
    db_out << "Successfully closed socket." << std::endl;
    return 0;

  }

  db_out << "Failed to close socket." << std::endl;
  return -1;
  }

};

#endif
