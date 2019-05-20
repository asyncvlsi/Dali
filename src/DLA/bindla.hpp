//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BINDLA_HPP
#define HPCC_BINDLA_HPP

#include <vector>

class bin_dla {
public:
  bin_dla();
  double left;
  double bottom;
  int width;
  int height;
  std::vector<int> CIB; // stands for cell in this bin, used to store the list of cells in this bin
};


#endif //HPCC_BINDLA_HPP
