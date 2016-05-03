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
  FindPi();
  ~FindPi();
  
  std::string sample(std::string input);
  std::string aggregate(std::vector<std::string> samples);

};

#endif
