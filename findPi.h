/** Add description

(c) 2016 Jamie Lesser, Emily Hoyt
*/
#ifndef FIND_PI
#define FIND_PI

#include <cstring>
#include <vector>
#include <algorithm>

class FindPi {
 public:
  FindPi(){}
  ~FindPi(){}
  
  std::string sample() {
    float x = std::rand()*2.0 / RAND_MAX - 1.0;
    float y = std::rand()*2.0 / RAND_MAX - 1.0;
    
    if((x*x + y*y) < 1) {
      return "1";
    }
    return "0";
  }
  
  std::string aggregate(std::vector<std::string> samples) {
    float sum = 0.0;
    
    for (size_t i = 0; i < samples.size(); i++) {
      sum += std::atoi(samples[i].c_str());
    }
    
    float pi = 4.0 * sum / samples.size();
    
    return std::to_string(pi);
  }
  
};

#endif
