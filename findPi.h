/** Add description

(c) 2016 Jamie Lesser, Emily Hoyt
*/
#ifndef FIND_PI
#define FIND_PI

#include <cstring>
#include <vector>
#include <algorithm>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>

class FindPi {
 public:
  FindPi(){
    struct addrinfo hints, *info, *p;
    int gai_result;

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    if ((gai_result = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
      exit(1);
    }

    int seed = std::hash<std::string>{}(info->ai_canonname) + time(NULL);

    for(p = info; p != NULL; p = p->ai_next) {
      printf("hostname: %s\n", p->ai_canonname);
      printf("seed: %d\n", seed);
    }

    freeaddrinfo(info);
    
    std::srand(seed);
  }

  ~FindPi(){}
  
  std::string sample() {

    int n = 100000000;
    int count = 0;

    float x; 
    float y; 
    

    for (int i = 0; i < n; i++) {
      x = std::rand()*2.0 / RAND_MAX - 1.0;
      y = std::rand()*2.0 / RAND_MAX - 1.0;
      if((x*x + y*y) < 1) {
	count++;
      }
    }

    return std::to_string(count/(float)n);

  }
  
  std::string aggregate(std::vector<std::string> samples) {
    float sum = 0.0;
    
    for (size_t i = 0; i < samples.size(); i++) {
      sum += std::stof(samples[i].c_str());
    }
    
    float pi = 4.0 * sum / samples.size();
    
    return std::to_string(pi);
  }
  
};

#endif
