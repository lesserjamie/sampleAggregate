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
