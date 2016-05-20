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
    struct addrinfo hints, *info;

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    getaddrinfo(hostname, "http", &hints, &info);

    int seed = std::hash<std::string>{}(info->ai_canonname) + time(NULL);
    freeaddrinfo(info);
    
    std::srand(seed);
  }

  ~FindPi(){}
  
  std::string sample() {
    std::string result;
    int n = 10000;
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

    result = "(" + std::to_string(count/(float)n) + ", " + std::to_string(1) + ")";

    return result;

  }
  
  std::vector<std::string> partialAggregate(std::vector<std::string> samples) {
    std::vector<std::string> results;
    std::string result;
    float sum = 0.0;
    float total = 0;
    float sample;
    int num;
    size_t pos;

    for (size_t i = 0; i < samples.size(); i++) {
      pos = samples[i].find_first_of(",");
      if (pos != std::string::npos) {
	sample = std::stof(samples[i].substr(1, pos - 1).c_str());
	num = std::stoi(samples[i].substr(pos + 2, samples[i].length() - pos - 3));
	sum += float(num)*sample;
	total += num;
      }
    }

    result = "(" + std::to_string(sum/(float)total) + ", " + std::to_string(total) + ")";
    results.push_back(result);
    return results;
  }

  std::string aggregate(std::vector<std::string> samples) {
    float sum = 0.0;
    int total = 0;
    float sample;
    int num;
    size_t pos;


    for (size_t i = 0; i < samples.size(); i++) {
      pos = samples[i].find_first_of(",");
      if (pos != std::string::npos) {
	sample = std::stof(samples[i].substr(1, pos - 1).c_str());
	num = std::stoi(samples[i].substr(pos + 2, samples[i].length() - pos - 3));
	sum += float(num)*sample;
	total += num;

      }

    }
    
    float pi = 4.0 * sum / total;
    
    return std::to_string(pi);
  }
  
};

#endif
