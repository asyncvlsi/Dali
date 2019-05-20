//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_DLABIN_HPP
#define HPCC_DLABIN_HPP

class dla_bin {
public:
  dla_bin();
  double left;
  double bottom;
  int width;
  int height;
  vector<int> CIB; // stands for cell in this bin, used to store the list of cells in this bin
};

#endif //HPCC_DLABIN_HPP
